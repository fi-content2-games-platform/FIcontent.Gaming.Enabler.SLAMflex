#include "SendMessage.h"
#include <stdlib.h>
#include "globals.h"

SendMessage::SendMessage()
{
	sendMessageToUnity = NULL;
	sendPoseToUnityPointer = NULL;
	sendArrayPointsToUnity = NULL;
	sendLogToUnity = NULL;
	arrayOfPointsData = NULL;
}

void SendMessage::SetSendMessagesFunc(sendMessageFunc smf, sendPoseFunc spf, sendArrayPoints sar, sendLogFunc sendLog)
{
	sendMessageToUnity = smf;
	sendPoseToUnityPointer = spf;
	sendArrayPointsToUnity = sar;
	sendLogToUnity = sendLog;

}

void SendMessage::SendLogToUnity(const char* st)
{
	if(sendLogToUnity != NULL && EnableLogging)
	{
		(sendLogToUnity)(st);
	}
}

void SendMessage::SendStringToUnity(const char* st)
{
	if(sendMessageToUnity != NULL)
	{
		(sendMessageToUnity)(st);
	}
}

void SendMessage::SendPoseToUnity(float r1, float r2, float r3, double t1, double t2, double t3)
{
	if(sendPoseToUnityPointer != NULL)
	{
		(sendPoseToUnityPointer)(r1, r2, r3, t1, t2, t3);
	}
}

void SendMessage::SendArrayOfPoints(int arrayOfPoints [], int size)
{
	if(sendArrayPointsToUnity != NULL)
	{
		(sendArrayPointsToUnity)(arrayOfPoints, size);
	}
}
