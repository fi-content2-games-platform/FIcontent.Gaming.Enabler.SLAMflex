// Copyright 2008 Isis Innovation Limited
#include "System.h"
#include <stdlib.h>
#include <vector>


#include "ATANCamera.h"
#include "MapMaker.h"
#include "Tracker.h"

using namespace CVD;
using namespace std;

vector<float> System::getCurrentPose()
{
  	SE3<> se3pose=mpTracker->GetCurrentPose();
	vector<float> pose(7);
	Vector<3> translation,rotation;
	
	translation=se3pose.get_translation();
	rotation=se3pose.get_rotation().ln();
	pose[0]=translation[0];
	pose[1]=translation[1];
	pose[2]=translation[2];
	pose[3]=norm(rotation)/3.14159*360.0;
	pose[4]=rotation[0]/pose[3];
	pose[5]=rotation[1]/pose[3];
	pose[6]=rotation[2]/pose[3];
	return pose;
}

System::System()
{
  mpCamera = new ATANCamera("Camera");
  mpMap = new Map;
  mpMapMaker = new MapMaker(*mpMap, *mpCamera);
  mpTracker = new Tracker(ImageRef(640,480), *mpCamera, *mpMap, *mpMapMaker);
  mimFrameBW.resize(ImageRef(640,480));
  mbDone = false;
};

void System::SetSendMessageToUnity(SendMessage sm)
{
	mpTracker->SetSendMessageToUnity(sm);
	mpMapMaker->SetSendMessageToUnity(sm);
}

void System::SendTrackerStartSig()
{
	mpTracker->StartTracking();
}

void System::SendTrackerKillSig()
{
	mpTracker->StopTracking();
}

DetectionState System::GetCurrentDetectionState()
{
	mpTracker->GetCurrentDetectionState();
}

void System::RunOneFrame(unsigned char *bwImage,uint hnd)
{
   // Grab video frame in black and white
   mimFrameBW.copy_from(BasicImage<byte>(bwImage,ImageRef(640,480)));

   mpTracker->TrackFrame(mimFrameBW, hnd,0);
	
   string s=mpTracker->GetMessageForUser();

}

void System::GUICommandCallBack(void *ptr, string sCommand, string sParams)
{
  if(sCommand=="quit" || sCommand == "exit")
    static_cast<System*>(ptr)->mbDone = true;
}

std::string System::getVCorners()
{
    return mpTracker->getVCorners();
}








