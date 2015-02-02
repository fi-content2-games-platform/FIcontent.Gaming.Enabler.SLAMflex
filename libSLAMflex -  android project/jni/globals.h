#ifndef GLOBAL_VARS
#define GLOBAL_VARS
#include "image.h"
#include "TooN.h"
#include "sys/types.h"


#include<android/log.h>
#define LOG_TAG "SLAMflex"
#define LOGV(LOG_TAG, ...) __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__)

#define NUMTRACKERCAMPARAMETERS 5
const TooN::Vector<NUMTRACKERCAMPARAMETERS> CameraParameters;


const double MapMakerMaxKFDistWiggleMult =2;
const int MapMakerPlaneAlignerRansacs = 100;
const double Reloc2MaxScore = 9e6;
const int TrackerDrawFASTCorners =0;
const int MaxInitialTrails = 100;
const int BundleMaxIterations = 10; //was 20
const double BundleUpdateSquaredConvergenceLimit = 1e-06;
const int BundleCout = 240;

const double MapMakerWiggleScale = 0.1;
const std::string BundleMEstimator = "Tukey";
const double BundleMinTukeySigma = 0.4;

const double TrackerRotationEstimatorBlur = 0.75;
const int TrackerUseRotationEstimator = 1;
const int TrackerMiniPatchMaxSSD = 100000;
const int TrackerCoarseMin = 10;
const int TrackerCoarseMax = 60;
const int TrackerCoarseRange = 30;
const int TrackerCoarseSubPixIts = 8;

const int TrackerDisableCoarse = 0;
const double TrackerCoarseMinVelocity = 0.006;
const int TrackerMaxPatchesPerFrame = 1000;
const std::string TrackerMEstimator = "Tukey";
const double TrackerTrackingQualityGood = 0.3;
const double TrackerTrackingQualityLost = 0.13;
const double MapMakerCandidateMinShiTomasiScore = 70;
const int nMaxSSDPerPixel  = 500;


//Slam Flex
const int ScreenWidth = 640;
const int ScreenHeight = 480;
const int DesiredNumberOfCorners = 1000;
const int DesiredNumberOfCornersOffset = 100;
const bool UseNumberOfCornersAdjustment = true;
const int CounterForConvergenceLimit = 10;
const bool EnableLogging = false;
const bool SendArrayOfPointsForCornersTex = false;


#endif






