// Copyright 2008 Isis Innovation Limited
//#import <OpenGLES/ES1/gl.h>
//#import <OpenGLES/ES1/glext.h>
#include "Tracker.h"
#include "MEstimator.h"
#include "ShiTomasi.h"
#include "SmallMatrixOpts.h"
#include "PatchFinder.h"
#include "TrackerData.h"
#include "globals.h"

#include "utility.h"
#include "fast_corner.h"
#include "vision.h"
#include "wls.h"

#include <fstream>
#include <fcntl.h>

#include <math.h>

using namespace CVD;
using namespace std;


// The constructor mostly sets up interal reference variables
// to the other classes..
Tracker::Tracker(ImageRef irVideoSize, const ATANCamera &c, Map &m, MapMaker &mm) :
mMap(m),
mMapMaker(mm),
mCamera(c),
mRelocaliser(mMap, mCamera),
mirSize(irVideoSize)
{
	mCurrentKF.bFixed = false;
	TrackerData::irImageSize = mirSize;
	
	mpSBILastFrame = NULL;
	mpSBIThisFrame = NULL;
	
	// Most of the initialisation is done in Reset()
	Reset();
}
template <typename T> string tostr(string w, const T& t) {
    ostringstream os;
    os<<w<<t;
    return os.str();
}
void Tracker::StartTracking() 
{
	mbUserPressedSpacebar=true;
}

void Tracker::StopTracking()
{
	mbUserPressedSpacebar=false;
	Reset();
}

void Tracker::SetSendMessageToUnity(SendMessage sm)
{
	sendToUnity = sm;
}

DetectionState Tracker::GetCurrentDetectionState()
{
	return currentState;
}


// Resets the tracker, wipes the map.
// This is the main Reset-handler-entry-point of the program! Other classes' resets propagate from here.
// It's always called in the Tracker's thread, often as a GUI command.
void Tracker::Reset()
{
    std::cout << "Tracker resetting..." << std::endl;
    LOGV(LOG_TAG, "Info: %s", "Tracker resetting...");
	mbDidCoarse = false;
	mbUserPressedSpacebar = false;
	mTrackingQuality = GOOD;
	mnLostFrames = 0;
	mdMSDScaledVelocityMagnitude = 0;
	mCurrentKF.dSceneDepthMean = 1.0;
	mCurrentKF.dSceneDepthSigma = 1.0;
	mnInitialStage = TRAIL_TRACKING_NOT_STARTED;
	mlTrails.clear();
	mCamera.SetImageSize(mirSize);
	mCurrentKF.mMeasurements.clear();
	mnLastKeyFrameDropped = -20;
	mnFrame=0;
	mv6CameraVelocity = Zeros;
	mbJustRecoveredSoUseCoarse = false;
	threshold = 10;
	thresholdReached = false;
	currentState = Finished;
	
	// Tell the MapMaker to reset itself.. 
	// this may take some time, since the mapmaker thread may have to wait
	// for an abort-check during calculation, so sleep while waiting.
	// MapMaker will also clear the map.
	mMapMaker.RequestReset();
	while(!mMapMaker.ResetDone())
		usleep(10);
	
    std::cout << "Tracker reset." << std::endl;
    LOGV(LOG_TAG, "Info: %s", "Tracker reset.");
    sendToUnity.SendStringToUnity("Tracker_Reset");
}

void Tracker::TrackFrame(Image<byte> &imFrame, uint hnd,bool bDraw)
{
	mbDraw = bDraw;
	mMessageForUser.str("");
	currentState = Started;

	// Take the input video image, and convert it into the tracker's keyframe struct
	// This does things like generate the image pyramid and find FAST corners
	mCurrentKF.mMeasurements.clear();
    // Finding corners
	int numCrns;
	numCrns = mCurrentKF.MakeKeyFrame_Lite(imFrame, threshold);

	if(UseNumberOfCornersAdjustment)
	{
		if (numCrns > (DesiredNumberOfCorners + DesiredNumberOfCornersOffset))
		{
			threshold+=5;
		}
		else if (numCrns < (DesiredNumberOfCorners - DesiredNumberOfCornersOffset))
		{
			if(threshold > 14)
			threshold -= 5;
		}
	}


	// Update the small images for the rotation estimator
	static double gvdSBIBlur = TrackerRotationEstimatorBlur;
	static int gvnUseSBI = TrackerUseRotationEstimator;
	mbUseSBIInit = gvnUseSBI;
	if(!mpSBIThisFrame)
	{
		mpSBIThisFrame = new SmallBlurryImage(mCurrentKF, gvdSBIBlur);
		mpSBILastFrame = new SmallBlurryImage(mCurrentKF, gvdSBIBlur);
	}
	else
	{
		delete  mpSBILastFrame;
		mpSBILastFrame = mpSBIThisFrame;
		mpSBIThisFrame = new SmallBlurryImage(mCurrentKF, gvdSBIBlur);
	}
	
	// From now on we only use the keyframe struct!
	mnFrame++;
	
	std::ostringstream points;
	points << "Corners: " << mCurrentKF.aLevels[0].vCorners.size() << "  Barrier: " << threshold << endl;
	sendToUnity.SendStringToUnity(points.str().c_str());

	//Sending corners to Unity3D for rendering
	if(SendArrayOfPointsForCornersTex)
	{
		int crnrs [mCurrentKF.aLevels[0].vCorners.size()];
		for (int i=0; i<mCurrentKF.aLevels[0].vCorners.size(); i++)
		{
			crnrs[i]=(-640*(mCurrentKF.aLevels[0].vCorners[i].y-480))+mCurrentKF.aLevels[0].vCorners[i].x;
		}

		sendToUnity.SendArrayOfPoints(crnrs, mCurrentKF.aLevels[0].vCorners.size());
	}

	// Decide what to do - if there is a map, try to track the map ...
	if(mMap.IsGood())
	{
		if(1)//mnLostFrames < 3)  // .. but only if we're not lost!
		{
//			cout << "----Map is good----"<<endl;
            if(mbUseSBIInit)
				CalcSBIRotation();
			ApplyMotionModel();       // 
			TrackMap();               //  These three lines do the main tracking work.
			UpdateMotionModel();      // 
			
			AssessTrackingQuality();  //  Check if we're lost or if tracking is poor.
			
			{ // Provide some feedback for the user:
				mMessageForUser << "Tracking Map, quality ";
				if(mTrackingQuality == GOOD)  mMessageForUser << "good.";
				if(mTrackingQuality == DODGY) mMessageForUser << "poor.";
				if(mTrackingQuality == BAD)   mMessageForUser << "bad.";
				mMessageForUser << " Found:";
				for(int i=0; i<LEVELS; i++) mMessageForUser << " " << manMeasFound[i] << "/" << manMeasAttempted[i];
				//	    mMessageForUser << " Found " << mnMeasFound << " of " << mnMeasAttempted <<". (";
				mMessageForUser << " Map: " << mMap.vpPoints.size() << "P, " << mMap.vpKeyFrames.size() << "KF";
			}
			
			// Heuristics to check if a key-frame should be added to the map:
			if(mTrackingQuality == GOOD &&
			   mMapMaker.NeedNewKeyFrame(mCurrentKF) &&
			   mnFrame - mnLastKeyFrameDropped > 20  &&
			   mMapMaker.QueueSize() < 3)
			{
				mMessageForUser << " Adding key-frame.";
				AddNewKeyFrame();
				LOGV(LOG_TAG, "Info_Key-Frame: %s", "Adding key-frame");
			};
		}
		else  // what if there is a map, but tracking has been lost?
		{
			mMessageForUser << "** Attempting recovery **.";
			if(AttemptRecovery())
			{
				TrackMap();
				AssessTrackingQuality();
			}
		}
        
        
        Matrix<3,3,double> rotMatrix = mse3CamFromWorld.get_rotation().get_matrix();
        Vector<3, double> transVector = mse3CamFromWorld.get_translation();
        float heading, attitude, bank = 0;
        GetEulerAnglesFromRotationMatrix(rotMatrix, heading , attitude, bank);
        std::ostringstream data;
        data <<RadiansToDegrees(bank)+180<<";"<<RadiansToDegrees(attitude)<<";"<<RadiansToDegrees(heading)<<":"<<transVector[0]<<";"<<transVector[1]<<";"<<transVector[2]<<endl;
        
        sendToUnity.SendPoseToUnity(RadiansToDegrees(bank)+180, RadiansToDegrees(attitude), RadiansToDegrees(heading), transVector[0], transVector[1], transVector[2]);

       // LOGV(LOG_TAG, "Pose: %s", data.str().c_str());
        currentState = InProgres;
	}
	else // If there is no map, try to make one.
        {
            cout << "Track for initial map"<<endl;
            TrackForInitialMap();
        }
	// GUI interface
	while(!mvQueuedCommands.empty())
	{
		GUICommandHandler(mvQueuedCommands.begin()->sCommand, mvQueuedCommands.begin()->sParams);
		mvQueuedCommands.erase(mvQueuedCommands.begin());
	}
};


/** this conversion uses conventions as described on page:
 *   http://www.euclideanspace.com/maths/geometry/rotations/euler/index.htm
 *   Coordinate System: right hand
 *   Positive angle: right hand
 *   Order of euler angles: heading first, then attitude, then bank
 *   matrix row column ordering:
 *   [m00 m01 m02]
 *   [m10 m11 m12]
 *   [m20 m21 m22]*/
void Tracker::GetEulerAnglesFromRotationMatrix(Matrix<3,3, double>  &m, float &heading, float &attitude, float &bank) {
    // Assuming the angles are in radians.
    if (m(1,0) > 0.998) { // singularity at north pole
        heading = atan2(m(0,2),m(2,2));
        attitude = M_PI_2;
        bank = 0;
        return;
    }
    if (m(1,0) < -0.998) { // singularity at south pole
        heading = atan2(m(0,2),m(2,2));
        attitude = - M_PI_2;
        bank = 0;
        return;
    }
    heading = atan2(-m(2,0),m(0,0));
    bank = atan2(-m(1,2),m(1,1));
    attitude = asin(m(1,0));
}

// Try to relocalise in case tracking was lost.
// Returns success or failure as a bool.
// Actually, the SBI relocaliser will almost always return true, even if
// it has no idea where it is, so graphics will go a bit 
// crazy when lost. Could use a tighter SSD threshold and return more false,
// but the way it is now gives a snappier response and I prefer it.
bool Tracker::AttemptRecovery()
{
	bool bRelocGood = mRelocaliser.AttemptRecovery(mCurrentKF);
	if(!bRelocGood)
		return false;
	
	SE3<> se3Best = mRelocaliser.BestPose();
	mse3CamFromWorld = mse3StartPos = se3Best;
	mv6CameraVelocity = Zeros;
	mbJustRecoveredSoUseCoarse = true;
	return true;
}

// GUI interface. Stuff commands onto the back of a queue so the tracker handles
// them in its own thread at the end of each frame. Note the charming lack of
// any thread safety (no lock on mvQueuedCommands).
void Tracker::GUICommandCallBack(void* ptr, string sCommand, string sParams)
{
	Command c;
	c.sCommand = sCommand;
	c.sParams = sParams;
	((Tracker*) ptr)->mvQueuedCommands.push_back(c);
}

// This is called in the tracker's own thread.
void Tracker::GUICommandHandler(string sCommand, string sParams)  // Called by the callback func..
{
	if(sCommand=="Reset")
    {
		Reset();
		return;
    }
	
	// KeyPress commands are issued by GLWindow
	if(sCommand=="KeyPress")
    {
		if(sParams == "Space")
		{
			mbUserPressedSpacebar = true;
		}
		else if(sParams == "r")
		{
			Reset();
		}
		else if(sParams == "q" || sParams == "Escape")
		{
			//GUI.ParseLine("quit");
		}
		return;
    }
	if((sCommand=="PokeTracker"))
    {
		mbUserPressedSpacebar = true;
		return;
    }
    
	
	cout << "! Tracker::GUICommandHandler: unhandled command "<< sCommand << endl;
	exit(1);
}; 

// Routine for establishing the initial map. This requires two spacebar presses from the user
// to define the first two key-frames. Salient points are tracked between the two keyframes
// using cheap frame-to-frame tracking (which is very brittle - quick camera motion will
// break it.) The salient points are stored in a list of `Trail' data structures.
// What action TrackForInitialMap() takes depends on the mnInitialStage enum variable..
void Tracker::TrackForInitialMap()
{
	// MiniPatch tracking threshhold.
	static int gvnMaxSSD = TrackerMiniPatchMaxSSD;
	MiniPatch::mnMaxSSD = gvnMaxSSD;
	
	// What stage of initial tracking are we at?
	if(mnInitialStage == TRAIL_TRACKING_NOT_STARTED) 
    {
		if(mbUserPressedSpacebar)  // First spacebar = this is the first keyframe
		{
			mbUserPressedSpacebar = false;
			TrailTracking_Start();
			mnInitialStage = TRAIL_TRACKING_STARTED;

			currentState = InProgres;
			thresholdReached = true;
		}
		else
			mMessageForUser << "Point camera at planar scene and press spacebar to start tracking for initial map." << endl;
		return;
    };
	
	if(mnInitialStage == TRAIL_TRACKING_STARTED)
    {
		int nGoodTrails = TrailTracking_Advance();  // This call actually tracks the trails
		if(nGoodTrails < 10) // if most trails have been wiped out, no point continuing.
		{
			sendToUnity.SendStringToUnity("Tracker_ResetMM");
			Reset();
			return;
		}
		
		// If the user pressed spacebar here, use trails to run stereo and make the intial map..
		if(mbUserPressedSpacebar)
		{
			clock_t startMapMaker, stopMapMaker;
			int numOfCorners;
			startMapMaker = clock();
			numOfCorners = mCurrentKF.aLevels[0].vCorners.size();

			mbUserPressedSpacebar = false;
			vector<pair<ImageRef, ImageRef> > vMatches;   // This is the format the mapmaker wants for the stereo pairs
			for(list<Trail>::iterator i = mlTrails.begin(); i!=mlTrails.end(); i++)
				vMatches.push_back(pair<ImageRef, ImageRef>(i->irInitialPos,
															i->irCurrentPos));
			bool result = mMapMaker.InitFromStereo(mFirstKF, mCurrentKF, vMatches, mse3CamFromWorld);  // This will take some time!
			if (!result)
			{
				Reset();
				LOGV(LOG_TAG, "MapMaker.InitFromStereo: Failed, Tracker: Reset()");
			}

			if(EnableLogging)
			{
				stopMapMaker = clock();
				std::ostringstream message;
				message <<"MapMaker: number of corners : "<< numOfCorners <<"::Number of trails: "<< vMatches.size()<< ":: Duration of initFromStereo: " << (float)(stopMapMaker - startMapMaker)/CLOCKS_PER_SEC*1000 << endl;
				sendToUnity.SendLogToUnity(message.str().c_str());
			}
			mnInitialStage = TRAIL_TRACKING_COMPLETE;
		}
		else
			mMessageForUser << "Translate the camera slowly sideways, and press spacebar again to perform stereo init." << endl;
    }
}

// The current frame is to be the first keyframe!
void Tracker::TrailTracking_Start()
{
	mCurrentKF.MakeKeyFrame_Rest();  // This populates the Candidates list, which is Shi-Tomasi thresholded.
	mFirstKF = mCurrentKF; 
	vector<pair<double,ImageRef> > vCornersAndSTScores;
	for(unsigned int i=0; i<mCurrentKF.aLevels[0].vCandidates.size(); i++)  // Copy candidates into a trivially sortable vector
    {                                                                     // so that we can choose the image corners with max ST score
		Candidate &c = mCurrentKF.aLevels[0].vCandidates[i];
		if(!mCurrentKF.aLevels[0].im.in_image_with_border(c.irLevelPos, MiniPatch::mnHalfPatchSize))
			continue;
		vCornersAndSTScores.push_back(pair<double,ImageRef>(-1.0 * c.dSTScore, c.irLevelPos)); // negative so highest score first in sorted list
    };
	sort(vCornersAndSTScores.begin(), vCornersAndSTScores.end());  // Sort according to Shi-Tomasi score
	int nToAdd = MaxInitialTrails;
	for(unsigned int i = 0; i<vCornersAndSTScores.size() && nToAdd > 0; i++)
    {
		if(!mCurrentKF.aLevels[0].im.in_image_with_border(vCornersAndSTScores[i].second, MiniPatch::mnHalfPatchSize))
			continue;
		Trail t;
		t.mPatch.SampleFromImage(vCornersAndSTScores[i].second, mCurrentKF.aLevels[0].im);
		t.irInitialPos = vCornersAndSTScores[i].second;
		t.irCurrentPos = t.irInitialPos;
		mlTrails.push_back(t);
		nToAdd--;
    }
	mPreviousFrameKF = mFirstKF;  // Always store the previous frame so married-matching can work.
}

// Steady-state trail tracking: Advance from the previous frame, remove duds.
int Tracker::TrailTracking_Advance()
{
	int nGoodTrails = 0;
	MiniPatch BackwardsPatch;
	Level &lCurrentFrame = mCurrentKF.aLevels[0];
	Level &lPreviousFrame = mPreviousFrameKF.aLevels[0];
    std::cout << "number of trails: " << mlTrails.size() << std::endl;
    LOGV(LOG_TAG, "number of trails: %i", mlTrails.size());
	
	for(list<Trail>::iterator i = mlTrails.begin(); i!=mlTrails.end();)
    {
		list<Trail>::iterator next = i; next++;
		
		Trail &trail = *i;
		ImageRef irStart = trail.irCurrentPos;
		ImageRef irEnd = irStart;
		bool bFound = trail.mPatch.FindPatch(irEnd, lCurrentFrame.im, 10, lCurrentFrame.vCorners);
		if(bFound)
		{
			// Also find backwards in a married-matches check
			BackwardsPatch.SampleFromImage(irEnd, lCurrentFrame.im);
			ImageRef irBackWardsFound = irEnd;
			bFound = BackwardsPatch.FindPatch(irBackWardsFound, lPreviousFrame.im, 10, lPreviousFrame.vCorners);
			if((irBackWardsFound - irStart).mag_squared() > 2)
				bFound = false;
			
			trail.irCurrentPos = irEnd;
			nGoodTrails++;
		}
		
		if(!bFound) // Erase from list of trails if not found this frame.
		{
			mlTrails.erase(i);
		}
		i = next;
    }
		
		mPreviousFrameKF = mCurrentKF;
	return nGoodTrails;
}

// TrackMap is the main purpose of the Tracker.
// It first projects all map points into the image to find a potentially-visible-set (PVS);
// Then it tries to find some points of the PVS in the image;
// Then it updates camera pose according to any points found.
// Above may happen twice if a coarse tracking stage is performed.
// Finally it updates the tracker's current-frame-KeyFrame struct with any
// measurements made.
// A lot of low-level functionality is split into helper classes:
// class TrackerData handles the projection of a MapPoint and stores intermediate results;
// class PatchFinder finds a projected MapPoint in the current-frame-KeyFrame.
void Tracker::TrackMap()
{
	if (mbUserPressedSpacebar) {
		StopTracking();
	}
	// Some accounting which will be used for tracking quality assessment:
	for(int i=0; i<LEVELS; i++)
		manMeasAttempted[i] = manMeasFound[i] = 0;
	
	// The Potentially-Visible-Set (PVS) is split into pyramid levels.
	vector<TrackerData*> avPVS[LEVELS]; 
	for(int i=0; i<LEVELS; i++)
		avPVS[i].reserve(500);
	
	// For all points in the map..
	for(unsigned int i=0; i<mMap.vpPoints.size(); i++)
    {
		MapPoint &p= *(mMap.vpPoints[i]); 
		// Ensure that this map point has an associated TrackerData struct.
		if(!p.pTData) p.pTData = new TrackerData(&p);   
		TrackerData &TData = *p.pTData;
		
		// Project according to current view, and if it's not in the image, skip.
		TData.Project(mse3CamFromWorld, mCamera); 
		if(!TData.bInImage)
			continue;   
		
		// Calculate camera projection derivatives of this point.
		TData.GetDerivsUnsafe(mCamera);
		
		// And check what the PatchFinder (included in TrackerData) makes of the mappoint in this view..
		TData.nSearchLevel = TData.Finder.CalcSearchLevelAndWarpMatrix(TData.Point, mse3CamFromWorld, TData.m2CamDerivs);
		if(TData.nSearchLevel == -1)
			continue;   // a negative search pyramid level indicates an inappropriate warp for this view, so skip.
		
		// Otherwise, this point is suitable to be searched in the current image! Add to the PVS.
		TData.bSearched = false;
		TData.bFound = false;
		avPVS[TData.nSearchLevel].push_back(&TData);
    };
	
	// Next: A large degree of faffing about and deciding which points are going to be measured!
	// First, randomly shuffle the individual levels of the PVS.
	for(int i=0; i<LEVELS; i++)
		random_shuffle(avPVS[i].begin(), avPVS[i].end());
	
	// The next two data structs contain the list of points which will next 
	// be searched for in the image, and then used in pose update.
	vector<TrackerData*> vNextToSearch;
	vector<TrackerData*> vIterationSet;
	
	// Tunable parameters to do with the coarse tracking stage:
	static unsigned int gvnCoarseMin=TrackerCoarseMin;   // Min number of large-scale features for coarse stage
	static unsigned int gvnCoarseMax=TrackerCoarseMax;   // Max number of large-scale features for coarse stage
	static unsigned int gvnCoarseRange=TrackerCoarseRange;       // Pixel search radius for coarse features
	static int gvnCoarseSubPixIts=TrackerCoarseSubPixIts; // Max sub-pixel iterations for coarse features
	static int gvnCoarseDisabled=TrackerDisableCoarse;    // Set this to 1 to disable coarse stage (except after recovery)
	static double gvdCoarseMinVel=TrackerCoarseMinVelocity;  // Speed above which coarse stage is used.
	
	unsigned int nCoarseMax = gvnCoarseMax;
	unsigned int nCoarseRange = gvnCoarseRange;
	
	mbDidCoarse = false;
	
	// Set of heuristics to check if we should do a coarse tracking stage.
	bool bTryCoarse = true;
	if(gvnCoarseDisabled || 
	   mdMSDScaledVelocityMagnitude < gvdCoarseMinVel  ||
	   nCoarseMax == 0)
		bTryCoarse = false;
	if(mbJustRecoveredSoUseCoarse)
    {
		bTryCoarse = true;
		nCoarseMax *=2;
		nCoarseRange *=2;
		mbJustRecoveredSoUseCoarse = false;
    };
	
	// If we do want to do a coarse stage, also check that there's enough high-level 
	// PV map points. We use the lowest-res two pyramid levels (LEVELS-1 and LEVELS-2),
	// with preference to LEVELS-1.
	if(bTryCoarse && avPVS[LEVELS-1].size() + avPVS[LEVELS-2].size() > gvnCoarseMin )
    {
		// Now, fill the vNextToSearch struct with an appropriate number of 
		// TrackerDatas corresponding to coarse map points! This depends on how many
		// there are in different pyramid levels compared to CoarseMin and CoarseMax.
		
		if(avPVS[LEVELS-1].size() <= nCoarseMax) 
		{ // Fewer than CoarseMax in LEVELS-1? then take all of them, and remove them from the PVS list.
			vNextToSearch = avPVS[LEVELS-1];
			avPVS[LEVELS-1].clear();
		}
		else
		{ // ..otherwise choose nCoarseMax at random, again removing from the PVS list.
			for(unsigned int i=0; i<nCoarseMax; i++)
				vNextToSearch.push_back(avPVS[LEVELS-1][i]);
			avPVS[LEVELS-1].erase(avPVS[LEVELS-1].begin(), avPVS[LEVELS-1].begin() + nCoarseMax);
		}
		
		// If didn't source enough from LEVELS-1, get some from LEVELS-2... same as above.
		if(vNextToSearch.size() < nCoarseMax)
		{
			unsigned int nMoreCoarseNeeded = nCoarseMax - vNextToSearch.size();
			if(avPVS[LEVELS-2].size() <= nMoreCoarseNeeded)
			{
				vNextToSearch = avPVS[LEVELS-2];
				avPVS[LEVELS-2].clear();
			}
			else
			{
				for(unsigned int i=0; i<nMoreCoarseNeeded; i++)
					vNextToSearch.push_back(avPVS[LEVELS-2][i]);
				avPVS[LEVELS-2].erase(avPVS[LEVELS-2].begin(), avPVS[LEVELS-2].begin() + nMoreCoarseNeeded);
			}
		}
		// Now go and attempt to find these points in the image!
		unsigned int nFound = SearchForPoints(vNextToSearch, nCoarseRange, gvnCoarseSubPixIts);
		vIterationSet = vNextToSearch;  // Copy over into the to-be-optimised list.
		if(nFound >= gvnCoarseMin)  // Were enough found to do any meaningful optimisation?
		{
			mbDidCoarse = true;
			for(int iter = 0; iter<10; iter++) // If so: do ten Gauss-Newton pose updates iterations.
			{
				if(iter != 0)
				{ // Re-project the points on all but the first iteration.
					for(unsigned int i=0; i<vIterationSet.size(); i++)
						if(vIterationSet[i]->bFound)  
							vIterationSet[i]->ProjectAndDerivs(mse3CamFromWorld, mCamera);
				}
				for(unsigned int i=0; i<vIterationSet.size(); i++)
					if(vIterationSet[i]->bFound)
						vIterationSet[i]->CalcJacobian();
				double dOverrideSigma = 0.0;
				// Hack: force the MEstimator to be pretty brutal 
				// with outliers beyond the fifth iteration.
				if(iter > 5)
					dOverrideSigma = 1.0;
				
				// Calculate and apply the pose update...
				Vector<6> v6Update = 
				CalcPoseUpdate(vIterationSet, dOverrideSigma);
				mse3CamFromWorld = SE3<>::exp(v6Update) * mse3CamFromWorld;
			};
		}
    };
	
	// So, at this stage, we may or may not have done a coarse tracking stage.
	// Now do the fine tracking stage. This needs many more points!
	
	int nFineRange = 10;  // Pixel search range for the fine stage. 
	if(mbDidCoarse)       // Can use a tighter search if the coarse stage was already done.
		nFineRange = 5;
	
	// What patches shall we use this time? The high-level ones are quite important,
	// so do all of these, with sub-pixel refinement.
	{
		int l = LEVELS - 1;
		for(unsigned int i=0; i<avPVS[l].size(); i++)
			avPVS[l][i]->ProjectAndDerivs(mse3CamFromWorld, mCamera);
		SearchForPoints(avPVS[l], nFineRange, 8);
		for(unsigned int i=0; i<avPVS[l].size(); i++)
			vIterationSet.push_back(avPVS[l][i]);  // Again, plonk all searched points onto the (maybe already populate) vIterationSet.
	};
	
	// All the others levels: Initially, put all remaining potentially visible patches onto vNextToSearch.
	vNextToSearch.clear();
	for(int l=LEVELS - 2; l>=0; l--)
		for(unsigned int i=0; i<avPVS[l].size(); i++)
			vNextToSearch.push_back(avPVS[l][i]);
	
	// But we haven't got CPU to track _all_ patches in the map - arbitrarily limit 
	// ourselves to 1000, and choose these randomly.
	static int gvnMaxPatchesPerFrame=TrackerMaxPatchesPerFrame;
	int nFinePatchesToUse = gvnMaxPatchesPerFrame - vIterationSet.size();
	if(nFinePatchesToUse < 0)
		nFinePatchesToUse = 0;
	if((int) vNextToSearch.size() > nFinePatchesToUse)
    {
		random_shuffle(vNextToSearch.begin(), vNextToSearch.end());
		vNextToSearch.resize(nFinePatchesToUse); // Chop!
    };
	
	// If we did a coarse tracking stage: re-project and find derivs of fine points
	if(mbDidCoarse)
		for(unsigned int i=0; i<vNextToSearch.size(); i++)
			vNextToSearch[i]->ProjectAndDerivs(mse3CamFromWorld, mCamera);
	
	// Find fine points in image:
	SearchForPoints(vNextToSearch, nFineRange, 0);
	// And attach them all to the end of the optimisation-set.
	for(unsigned int i=0; i<vNextToSearch.size(); i++)
		vIterationSet.push_back(vNextToSearch[i]);
	
	// Again, ten gauss-newton pose update iterations.
	Vector<6> v6LastUpdate;
	v6LastUpdate = Zeros;
	for(int iter = 0; iter<10; iter++)
    {
		bool bNonLinearIteration; // For a bit of time-saving: don't do full nonlinear
								  // reprojection at every iteration - it really isn't necessary!
		if(iter == 0 || iter == 4 || iter == 9)
			bNonLinearIteration = true;   // Even this is probably overkill, the reason we do many
		else                            // iterations is for M-Estimator convergence rather than 
			bNonLinearIteration = false;  // linearisation effects.
		
		if(iter != 0)   // Either way: first iteration doesn't need projection update.
        {
			if(bNonLinearIteration)
			{
				for(unsigned int i=0; i<vIterationSet.size(); i++)
					if(vIterationSet[i]->bFound)
						vIterationSet[i]->ProjectAndDerivs(mse3CamFromWorld, mCamera);
			}
			else
			{
				for(unsigned int i=0; i<vIterationSet.size(); i++)
					if(vIterationSet[i]->bFound)
						vIterationSet[i]->LinearUpdate(v6LastUpdate);
			};
		}
		
		if(bNonLinearIteration)
			for(unsigned int i=0; i<vIterationSet.size(); i++)
				if(vIterationSet[i]->bFound)
					vIterationSet[i]->CalcJacobian();
		
		// Again, an M-Estimator hack beyond the fifth iteration.
		double dOverrideSigma = 0.0;
		if(iter > 5)
			dOverrideSigma = 16.0;
		
		// Calculate and update pose; also store update vector for linear iteration updates.
		Vector<6> v6Update = 
		CalcPoseUpdate(vIterationSet, dOverrideSigma, iter==9);
		mse3CamFromWorld = SE3<>::exp(v6Update) * mse3CamFromWorld;
		v6LastUpdate = v6Update;
    };
	
	// Update the current keyframe with info on what was found in the frame.
	// Strictly speaking this is unnecessary to do every frame, it'll only be
	// needed if the KF gets added to MapMaker. Do it anyway.
	// Export pose to current keyframe:
	mCurrentKF.se3CfromW = mse3CamFromWorld;
	
	// Record successful measurements. Use the KeyFrame-Measurement struct for this.
	mCurrentKF.mMeasurements.clear();
	for(vector<TrackerData*>::iterator it = vIterationSet.begin();
		it!= vIterationSet.end(); 
		it++)
    {
		if(! (*it)->bFound)
			continue;
		Measurement m;
		m.v2RootPos = (*it)->v2Found;
		m.nLevel = (*it)->nSearchLevel;
		m.bSubPix = (*it)->bDidSubPix; 
		mCurrentKF.mMeasurements[& ((*it)->Point)] = m;
    }
	
	// Finally, find the mean scene depth from tracked features
	{
		double dSum = 0;
		double dSumSq = 0;
		int nNum = 0;
		for(vector<TrackerData*>::iterator it = vIterationSet.begin();
			it!= vIterationSet.end(); 
			it++)
			if((*it)->bFound)
			{
				double z = (*it)->v3Cam[2];
				dSum+= z;
				dSumSq+= z*z;
				nNum++;
			};
		if(nNum > 20)
		{
			mCurrentKF.dSceneDepthMean = dSum/nNum;
			mCurrentKF.dSceneDepthSigma = sqrt((dSumSq / nNum) - (mCurrentKF.dSceneDepthMean) * (mCurrentKF.dSceneDepthMean));
		}
	}
}

// Find points in the image. Uses the PatchFiner struct stored in TrackerData
int Tracker::SearchForPoints(vector<TrackerData*> &vTD, int nRange, int nSubPixIts)
{
	int nFound = 0;
	for(unsigned int i=0; i<vTD.size(); i++)   // for each point..
    {
		// First, attempt a search at pixel locations which are FAST corners.
		// (PatchFinder::FindPatchCoarse)
		TrackerData &TD = *vTD[i];
		PatchFinder &Finder = TD.Finder;
		Finder.MakeTemplateCoarseCont(TD.Point);
		if(Finder.TemplateBad())
		{
			TD.bInImage = TD.bPotentiallyVisible = TD.bFound = false;
			continue;
		}
		manMeasAttempted[Finder.GetLevel()]++;  // Stats for tracking quality assessmenta
		
		bool bFound = 
		Finder.FindPatchCoarse(ir(TD.v2Image), mCurrentKF, nRange);
		TD.bSearched = true;
		if(!bFound) 
		{
			TD.bFound = false;
			continue;
		}
		
		TD.bFound = true;
		TD.dSqrtInvNoise = (1.0 / Finder.GetLevelScale());
		
		nFound++;
		manMeasFound[Finder.GetLevel()]++;
		
		// Found the patch in coarse search - are Sub-pixel iterations wanted too?
		if(nSubPixIts > 0)
		{
			TD.bDidSubPix = true;
			Finder.MakeSubPixTemplate();
			bool bSubPixConverges=Finder.IterateSubPixToConvergence(mCurrentKF, nSubPixIts);
			if(!bSubPixConverges)
			{ // If subpix doesn't converge, the patch location is probably very dubious!
				TD.bFound = false;
				nFound--;
				manMeasFound[Finder.GetLevel()]--;
				continue;
			}
			TD.v2Found = Finder.GetSubPixPos();
		}
		else
		{
			TD.v2Found = Finder.GetCoarsePosAsVector();
			TD.bDidSubPix = false;
		}
    }
	return nFound;
};

//Calculate a pose update 6-vector from a bunch of image measurements.
//User-selectable M-Estimator.
//Normally this robustly estimates a sigma-squared for all the measurements
//to reduce outlier influence, but this can be overridden if
//dOverrideSigma is positive. Also, bMarkOutliers set to true
//records any instances of a point being marked an outlier measurement
//by the Tukey MEstimator.
Vector<6> Tracker::CalcPoseUpdate(vector<TrackerData*> vTD, double dOverrideSigma, bool bMarkOutliers)
{
	// Which M-estimator are we using?
	int nEstimator = 0;
	static string gvsEstimator=TrackerMEstimator;
	if(gvsEstimator == "Tukey")
		nEstimator = 0;
	else if(gvsEstimator == "Cauchy")
		nEstimator = 1;
	else if(gvsEstimator == "Huber")
		nEstimator = 2;
	else 
    {
		cout << "Invalid TrackerMEstimator, choices are Tukey, Cauchy, Huber" << endl;
		nEstimator = 0;
		gvsEstimator = "Tukey";
    };
	
	// Find the covariance-scaled reprojection error for each measurement.
	// Also, store the square of these quantities for M-Estimator sigma squared estimation.
	vector<double> vdErrorSquared;
	for(unsigned int f=0; f<vTD.size(); f++)
    {
		TrackerData &TD = *vTD[f];
		if(!TD.bFound)
			continue;
		TD.v2Error_CovScaled = TD.dSqrtInvNoise* (TD.v2Found - TD.v2Image);
		vdErrorSquared.push_back(TD.v2Error_CovScaled * TD.v2Error_CovScaled);
    };
	
	// No valid measurements? Return null update.
	if(vdErrorSquared.size() == 0)
		return makeVector( 0,0,0,0,0,0);
	
	// What is the distribution of errors?
	double dSigmaSquared;
	if(dOverrideSigma > 0)
		dSigmaSquared = dOverrideSigma; // Bit of a waste having stored the vector of square errors in this case!
	else
    {
		if (nEstimator == 0)
			dSigmaSquared = Tukey::FindSigmaSquared(vdErrorSquared);
		else if(nEstimator == 1)
			dSigmaSquared = Cauchy::FindSigmaSquared(vdErrorSquared);
		else 
			dSigmaSquared = Huber::FindSigmaSquared(vdErrorSquared);
    }
	
	// The TooN WLSCholesky class handles reweighted least squares.
	// It just needs errors and jacobians.
	WLS<6> wls;
	wls.add_prior(100.0); // Stabilising prior
	for(unsigned int f=0; f<vTD.size(); f++)
    {
		TrackerData &TD = *vTD[f];
		if(!TD.bFound)
			continue;
		Vector<2> &v2 = TD.v2Error_CovScaled;
		double dErrorSq = v2 * v2;
		double dWeight;
		
		if(nEstimator == 0)
			dWeight= Tukey::Weight(dErrorSq, dSigmaSquared);
		else if(nEstimator == 1)
			dWeight= Cauchy::Weight(dErrorSq, dSigmaSquared);
		else 
			dWeight= Huber::Weight(dErrorSq, dSigmaSquared);
		
		// Inlier/outlier accounting, only really works for cut-off estimators such as Tukey.
		if(dWeight == 0.0)
		{
			if(bMarkOutliers)
				TD.Point.nMEstimatorOutlierCount++;
			continue;
		}
		else
			if(bMarkOutliers)
				TD.Point.nMEstimatorInlierCount++;
		
		Matrix<2,6> &m26Jac = TD.m26Jacobian;
		wls.add_mJ(v2[0], TD.dSqrtInvNoise * m26Jac[0], dWeight); // These two lines are currently
		wls.add_mJ(v2[1], TD.dSqrtInvNoise * m26Jac[1], dWeight); // the slowest bit of poseits
    }
	
	wls.compute();
	return wls.get_mu();
}


// Just add the current velocity to the current pose.
// N.b. this doesn't actually use time in any way, i.e. it assumes
// a one-frame-per-second camera. Skipped frames etc
// are not handled properly here.
void Tracker::ApplyMotionModel()
{
	mse3StartPos = mse3CamFromWorld;
	Vector<6> v6Velocity = mv6CameraVelocity;
	if(mbUseSBIInit)
    {
		v6Velocity.slice<3,3>() = mv6SBIRot.slice<3,3>();
		v6Velocity[0] = 0.0;
		v6Velocity[1] = 0.0;
    }
	mse3CamFromWorld = SE3<>::exp(v6Velocity) * mse3StartPos;
};


// The motion model is entirely the tracker's, and is kept as a decaying
// constant velocity model.
void Tracker::UpdateMotionModel()
{
	SE3<> se3NewFromOld = mse3CamFromWorld * mse3StartPos.inverse();
	Vector<6> v6Motion = SE3<>::ln(se3NewFromOld);
	Vector<6> v6OldVel = mv6CameraVelocity;
	
	mv6CameraVelocity = 0.9 * (0.5 * v6Motion + 0.5 * v6OldVel);
	mdVelocityMagnitude = sqrt(mv6CameraVelocity * mv6CameraVelocity);
	
	// Also make an estimate of this which has been scaled by the mean scene depth.
	// This is used to decide if we should use a coarse tracking stage.
	// We can tolerate more translational vel when far away from scene!
	Vector<6> v6 = mv6CameraVelocity;
	v6.slice<0,3>() *= 1.0 / mCurrentKF.dSceneDepthMean;
	mdMSDScaledVelocityMagnitude = sqrt(v6*v6);
}

// Time to add a new keyframe? The MapMaker handles most of this.
void Tracker::AddNewKeyFrame()
{
	mMapMaker.AddKeyFrame(mCurrentKF);
	mnLastKeyFrameDropped = mnFrame;
}

// Some heuristics to decide if tracking is any good, for this frame.
// This influences decisions to add key-frames, and eventually
// causes the tracker to attempt relocalisation.
void Tracker::AssessTrackingQuality()
{
	int nTotalAttempted = 0;
	int nTotalFound = 0;
	int nLargeAttempted = 0;
	int nLargeFound = 0;
	
	for(int i=0; i<LEVELS; i++)
    {
		nTotalAttempted += manMeasAttempted[i];
		nTotalFound += manMeasFound[i];
		if(i>=2) nLargeAttempted += manMeasAttempted[i];
		if(i>=2) nLargeFound += manMeasFound[i];
    }
	
	if(nTotalFound == 0 || nTotalAttempted == 0)
		mTrackingQuality = BAD;
	else
    {
		double dTotalFracFound = (double) nTotalFound / nTotalAttempted;
		double dLargeFracFound;
		if(nLargeAttempted > 10)
			dLargeFracFound = (double) nLargeFound / nLargeAttempted;
		else
			dLargeFracFound = dTotalFracFound;
		
		static double gvdQualityGood=TrackerTrackingQualityGood;
		static double gvdQualityLost=TrackerTrackingQualityLost;
		
		
		if(dTotalFracFound > gvdQualityGood)
			mTrackingQuality = GOOD;
		else if(dLargeFracFound < gvdQualityLost)
			mTrackingQuality = BAD;
		else
			mTrackingQuality = DODGY;
    }
	
	if(mTrackingQuality == DODGY)
    {
		// Further heuristics to see if it's actually bad, not just dodgy...
		// If the camera pose estimate has run miles away, it's probably bad.
		if(mMapMaker.IsDistanceToNearestKeyFrameExcessive(mCurrentKF))
			mTrackingQuality = BAD;
    }
	
	if(mTrackingQuality==BAD)
		mnLostFrames++;
	else
		mnLostFrames = 0;
}

string Tracker::GetMessageForUser()
{
	return mMessageForUser.str();
}

void Tracker::CalcSBIRotation()
{
	mpSBILastFrame->MakeJacs();
	pair<SE2<>, double> result_pair;
	result_pair = mpSBIThisFrame->IteratePosRelToTarget(*mpSBILastFrame, 6);
	SE3<> se3Adjust = SmallBlurryImage::SE3fromSE2(result_pair.first, mCamera);
	mv6SBIRot = se3Adjust.ln();
}

ImageRef TrackerData::irImageSize;  // Static member of TrackerData lives here

std::string Tracker::getVCorners()
{
    std::ostringstream corners;
//    for (int i=0; i<mCurrentKF.aLevels[0].vCorners.size(); i++)//mCurrentKF.aLevels[0].vCorners.size()
//    {
//        corners << mCurrentKF.aLevels[0].vCorners[i].x <<","<< mCurrentKF.aLevels[0].vCorners[i].y<<";";
//    }
    //cout <<   mCurrentKF.aLevels[0].vCorners.size()/2 << endl;
    corners << mCurrentKF.aLevels[0].vCorners.size();
    return corners.str();
}






