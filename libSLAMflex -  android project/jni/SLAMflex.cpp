#include <iostream>
#include <sstream>
#include <string>
#include <time.h>


#import "TooN.h"
#import "tracker.h"
#import "byte.h"
#include "System.h"
#include "globals.h"
#include <string.h>
#include "SendMessage.h"
#include "SLAMflex.h"

System ptam;  //PTAM system, whole plane detection is located here
SendMessage SM; // used for communication between library and Unity3D

unsigned char *bwImage; // Black and white image


//Convert Unity3D web texture image to black and white image
void convertToBlackWhite (unsigned char * pixels)
{
    memcpy(bwImage,pixels,640*480*4);

	unsigned int * pntrBWImage= (unsigned int *)bwImage;
	unsigned int index=0;
	unsigned int fourBytes;
	for (int j=0;j<480; j++)
	{
		for (int i=0; i<640; i++)
		{
			index=(640)*j+i;
			fourBytes=pntrBWImage[index];
			bwImage[index]=((unsigned char)fourBytes>>(2*8)) +((unsigned char)fourBytes>>(1*8))+((unsigned char)fourBytes>>(0*8));
		}
	}
}

void flip_vertically(unsigned char *pixels, const size_t width, const size_t height, const size_t bytes_per_pixel)
{
    const size_t stride = width * bytes_per_pixel;
    unsigned char *row = (unsigned char*)malloc(stride);
    unsigned char *low = pixels;
    unsigned char *high = &pixels[(height - 1) * stride];

    for (; low < high; low += stride, high -= stride) {
        memcpy(row, low, stride);
        memcpy(low, high, stride);
        memcpy(high, row, stride);
    }
    free(row);
}

//Change image formats from RGBA to BGRA
void rgba_to_bgra(unsigned char *pixels, int width, int height)
{
     for (int i = 0; i<width*height*4; i+=4)
    {
        std::swap(pixels[i], pixels[i+2]);

    }
}
//Exported functions from libSlamflex library
extern "C" {

	//Setup slamflex and connect to unity3D
	void _StartSlam(void* pointerString, void* pointerPose, void* pointerStringLog, void* pointerArrayOfPoints)
    {
        std::cout << "=> Start SlamFlex service" << std::endl;
        LOGV(LOG_TAG, "Info: %s", "=> Start SlamFlex service");
        SM.SetSendMessagesFunc((sendMessageFunc)pointerString, (sendPoseFunc)pointerPose, (sendArrayPoints)pointerArrayOfPoints, (sendLogFunc)pointerStringLog);
        ptam.SetSendMessageToUnity(SM);
        bwImage = (unsigned char *)malloc(ScreenHeight*ScreenWidth*4);
        memset(bwImage, 0, ScreenHeight*ScreenWidth*4);
    }

//Initiate plane detection
    void _StartPlaneDetection()
    {
        ptam.SendTrackerStartSig();
        std::cout << "-----Signal--StatusBar------" << std::endl;
        LOGV(LOG_TAG, "Info: %s", "-----Signal--StatusBar------");
    }

 //Function called each frame with pointer to image and width, height of image
	const char* _SetNewFrame (void* pointer, int width, int height)
	{
		unsigned char* data = reinterpret_cast<unsigned char*>(pointer);
        rgba_to_bgra(data, 640, 480);
        convertToBlackWhite(data);

        clock_t  tFrameStart, tFrameEnd;
        tFrameStart = clock();

        ptam.RunOneFrame(bwImage, 1);

        tFrameEnd = clock();;

        if(EnableLogging)
        {
			std::ostringstream message;
			message <<"Number Of Corners::   "<<ptam.getVCorners()<<"       :: Duration:  " << (float)(tFrameEnd - tFrameStart)/CLOCKS_PER_SEC*1000 << std::endl;
			SM.SendLogToUnity(message.str().c_str());
        }

        return (const char *[]){"Started", "InProgres", "Stopped", "Finished"}[ptam.GetCurrentDetectionState()];
	}
//Stops slam plane detection
	void _StopSlam()
	{
        std::cout << "=> _Stop SlamFlex" << std::endl;
        LOGV(LOG_TAG, "Info: %s", "=> _Stop SlamFlex");
        ptam.SendTrackerKillSig();
	}

}
