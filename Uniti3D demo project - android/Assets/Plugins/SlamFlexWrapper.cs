using UnityEngine;
using System.Collections;
using System.Runtime.InteropServices;
using System;

public class SlamFlexWrapper {

	public enum DetectionState
	{
		Started, 
		InProgres, 
		Stopped, 
		Finished
	};

	public delegate void SendStringDelegate(string s);
	public delegate void SendPoseDelegate(float r1, float r2, float r3, double t1, double t2, double t3);
	public delegate void SendLogDelegate(string text);
	public delegate void SendArrayPointsDelegate(IntPtr pointer, int size);

	/* Interface to native implementation */
	#if UNITY_IOS

	[DllImport ("__Internal")]
	private static extern void _StartSlam ();

	[DllImport ("__Internal")]
	private static extern IntPtr _SetNewFrame (IntPtr pointer, int width, int height);

	[DllImport ("__Internal")]
	private static extern void _StopSlam ();

	[DllImport ("__Internal")]
	private static extern void _StartPlaneDetection ();

#elif UNITY_ANDROID

	[DllImport ("slamflex")]
	private static extern void _StartSlam (SendStringDelegate ssd, SendPoseDelegate spd, SendLogDelegate sLogd, SendArrayPointsDelegate arrayPoints);
	
	[DllImport ("slamflex")]
	private static extern IntPtr _SetNewFrame (IntPtr pointer, int width, int height);
	
	[DllImport ("slamflex")]
	private static extern void _StopSlam ();
	
	[DllImport ("slamflex")]
	private static extern void _StartPlaneDetection ();

#endif

	/* Public interface for use inside C# / JS code */
	
	// Starts Slam detection
	public static void StartSlam(SendStringDelegate ssd, SendPoseDelegate spd, SendLogDelegate sLogd, SendArrayPointsDelegate arrayPoints)
	{
		// Call plugin only when running on real device
		if (ViablePlatform())
		{
			_StartSlam(ssd, spd, sLogd, arrayPoints);
		}

	}
	
	// Stops Slam detection
	public static void StopSlam()
	{
		// Call plugin only when running on real device
		if (ViablePlatform())
		{
			_StopSlam();
		}
	}
		
	public static DetectionState SetNewFrame(IntPtr pointer, int width, int height)
	{
//		 Call plugin only when running on real device
		if (ViablePlatform())
		{
			DetectionState det_state = DetectionState.Stopped;
			string state = Marshal.PtrToStringAnsi(_SetNewFrame(pointer, width, height));
			switch (state)
			{
				case "Started":
				{
					det_state = DetectionState.Started;
					break;
				}
				case "InProgres":
				{
					det_state = DetectionState.InProgres;
					break;
				}
				case "Stopped":
				{
					det_state = DetectionState.Stopped;
					break;
				}
				case "Finished":
				{
					det_state = DetectionState.Finished;
					break;
				}
				default:
				{
					det_state = DetectionState.Finished;
					break;
				}

			}

			//det_state = (DetectionState)Enum.Parse(typeof(DetectionState), state);

			return det_state;
		}
		// Return mockup values for code running inside Editor
		else
		{
			return DetectionState.Finished;
		}
	}

	//Start Slam detection
	public static void StartPlaneDetection()
	{
		if (ViablePlatform()) 
		{
			_StartPlaneDetection();
		} 
	}

	//Viable platform for running plugin
	private static bool ViablePlatform()
	{
		return (Application.platform == RuntimePlatform.Android || Application.platform == RuntimePlatform.IPhonePlayer);
	}


}