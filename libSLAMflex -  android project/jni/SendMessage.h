// -*- c++ -*-
//
#ifndef __SEND_MESSAGE
#define __SEND_MESSAGE

typedef void (*sendMessageFunc )(const char*);
typedef void (*sendLogFunc )(const char*);
typedef void (*sendPoseFunc )(float, float, float, double, double, double);
typedef void (*sendArrayPoints)(int[], int size);

class SendMessage
{
public:
  SendMessage();
  void SetSendMessagesFunc(sendMessageFunc smf, sendPoseFunc spf, sendArrayPoints sar, sendLogFunc sendLog);
  void SendStringToUnity(const char* st);
  void SendLogToUnity(const char* st);
  void SendPoseToUnity(float r1, float r2, float r3, double t1, double t2, double t3);
  void SendArrayOfPoints(int arrayOfPoint [], int size);
	
private:
  sendMessageFunc sendMessageToUnity;
  sendPoseFunc sendPoseToUnityPointer;
  sendArrayPoints sendArrayPointsToUnity;
  sendLogFunc sendLogToUnity;
  int* arrayOfPointsData;
};



#endif
