#include "SendMessage.h"
#include <stdlib.h>
#include "globals.h"
/// <summary>
/// SendMessage implements communication protocol between plugin and Unity3D
/// </summary>
SendMessage::SendMessage()
{
	sendMessageToUnity = NULL;
	sendPoseToUnityPointer = NULL;
	sendArrayPointsToUnity = NULL;
	sendLogToUnity = NULL;
	arrayOfPointsData = NULL;
}
/// <summary>
/// Sets callback functions recived from Unity3D
/// </summary>
/// <param name="smf">address of SendStringDelegate implemented in Unity3D</param>
/// <param name="spf">address of SendPoseDelegate implemented in Unity3D</param>
/// <param name="sendLog">address of SendLogDelegate implemented in Unity3D</param>
/// <param name="sar">address of SendArrayPointsDelegate implemented in Unity3D</param>
void SendMessage::SetSendMessagesFunc(sendMessageFunc smf, sendPoseFunc spf, sendArrayPoints sar, sendLogFunc sendLog)
{
	sendMessageToUnity = smf;
	sendPoseToUnityPointer = spf;
	sendArrayPointsToUnity = sar;
	sendLogToUnity = sendLog;

}
/// <summary>
/// Send string to Unity3D
/// </summary>
/// <param name="st">message to send to log in Unity3D</param>
void SendMessage::SendLogToUnity(const char* st)
{
	if(sendLogToUnity != NULL && EnableLogging)
	{
		(sendLogToUnity)(st);
	}
}

/// <summary>
/// Send string to Unity3D
/// </summary>
/// <param name="st">message to send to Unity3D</param>
void SendMessage::SendStringToUnity(const char* st)
{
	if(sendMessageToUnity != NULL)
	{
		(sendMessageToUnity)(st);
	}
}

/// <summary>
/// Send pose to Unity3D
/// </summary>
/// <param name="r1">rotation euler angle for unity game object transform X</param>
/// <param name="r2">rotation euler angle for unity game object transform Y</param>
/// <param name="r3">rotation euler angle for unity game object transform Z</param>
/// <param name="t1">translation for unity game object transform X</param>
/// <param name="t2">translation for unity game object transform Y</param>
/// <param name="t3">translation for unity game object transform Z</param>
void SendMessage::SendPoseToUnity(float r1, float r2, float r3, double t1, double t2, double t3)
{
	if(sendPoseToUnityPointer != NULL)
	{
		(sendPoseToUnityPointer)(r1, r2, r3, t1, t2, t3);
	}
}
/// <summary>
///  Send array of points to Unity3D
/// </summary>
/// <param name="arrayOfPoints">array of points to send</param>
/// <param name="size">number of elements</param>
void SendMessage::SendArrayOfPoints(int arrayOfPoints [], int size)
{
	if(sendArrayPointsToUnity != NULL)
	{
		(sendArrayPointsToUnity)(arrayOfPoints, size);
	}
}
