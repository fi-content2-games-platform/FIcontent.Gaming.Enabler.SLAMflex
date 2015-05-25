using UnityEngine;
using System.Collections;
using System.Runtime.InteropServices;
using System;
/// <summary>
/// SlamFlexWrapper implements interface to native implementation and public interface for use inside C# and JS code
/// </summary>
public class SlamFlexWrapper {
    /// <summary>
    /// Enum for state of SLAM detection process
    /// </summary>
	public enum DetectionState
	{
		Started, 
		InProgres, 
		Stopped, 
		Finished
	};
    /// <summary>
    /// Definition of delegate used from plugin to send string to Unity3D
    /// </summary>
    /// <param name="s">string to send</param>
	public delegate void SendStringDelegate(string s);
    /// <summary>
    /// Definition of delegate used from plugin to send pose to Unity3D
    /// </summary>
    /// <param name="r1">rotation euler angle for unity game object transform X</param>
    /// <param name="r2">rotation euler angle for unity game object transform Y</param>
    /// <param name="r3">rotation euler angle for unity game object transform Z</param>
    /// <param name="t1">translation for unity game object transform X</param>
    /// <param name="t2">translation for unity game object transform Y</param>
    /// <param name="t3">translation for unity game object transform Z</param>
	public delegate void SendPoseDelegate(float r1, float r2, float r3, double t1, double t2, double t3);
    /// <summary>
    ///  Definition of delegate used from plugin to send log to Unity3D
    /// </summary>
    /// <param name="text">string message to log</param>
	public delegate void SendLogDelegate(string text);
    /// <summary>
    ///  Definition of delegate used from plugin to send array of points to Unity3D
    /// </summary>
    /// <param name="pointer">Address of array</param>
    /// <param name="size">number of elements</param>
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
	
    /// <summary>
    /// Starts SLAM detection
    /// </summary>
    /// <param name="ssd">address of SendStringDelegate implemented in Unity3D</param>
    /// <param name="spd">address of SendPoseDelegate implemented in Unity3D</param>
    /// <param name="sLogd">address of SendLogDelegate implemented in Unity3D</param>
    /// <param name="arrayPoints">address of SendArrayPointsDelegate implemented in Unity3D</param>
	public static void StartSlam(SendStringDelegate ssd, SendPoseDelegate spd, SendLogDelegate sLogd, SendArrayPointsDelegate arrayPoints)
	{
		// Call plugin only when running on real device
		if (ViablePlatform())
		{
			_StartSlam(ssd, spd, sLogd, arrayPoints);
		}

	}


    /// <summary>
    /// Stops SLAM detection
    /// </summary>
	public static void StopSlam()
	{
		// Call plugin only when running on real device
		if (ViablePlatform())
		{
			_StopSlam();
		}
	}
	/// <summary>
    /// Send image data to plugin for SLAM detection
	/// </summary>
	/// <param name="pointer">image addess, pointer to first byte</param>
	/// <param name="width">image width</param>
	/// <param name="height">image height</param>
	/// <returns>current state of detection</returns>
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

    /// <summary>
    /// Initiate SLAM detection
    /// </summary>
	public static void StartPlaneDetection()
	{
		if (ViablePlatform()) 
		{
			_StartPlaneDetection();
		} 
	}

    /// <summary>
    /// Check if is viable platform for running plugin
    /// </summary>
    /// <returns></returns>
	private static bool ViablePlatform()
	{
		return (Application.platform == RuntimePlatform.Android || Application.platform == RuntimePlatform.IPhonePlayer);
	}


}