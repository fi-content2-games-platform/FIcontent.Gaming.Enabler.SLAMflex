/// Gyroscope-controlled camera for iPhone  Android revised 2.26.12
/// Perry Hoberman <hoberman@bway.net>
///
/// Usage:
////Attach this script to main camera.
/// Note: Unity Remote does not currently support gyroscope.
///
/// This script uses three techniques to get the correct orientation out of the gyroscope attitude:
/// 1. creates a parent transform (camParent) and rotates it with eulerAngles
/// 2. for Android (Samsung Galaxy Nexus) only: remaps gyro.Attitude quaternion values from xyzw to wxyz (quatMap)
/// 3. multiplies attitude quaternion by quaternion quatMult
/// Also creates a grandparent (camGrandparent) which can be rotated with localEulerAngles.y
/// This node allows an arbitrary heading to be added to the gyroscope reading
/// so that the virtual camera can be facing any direction in the scene, no matter what the phone's heading
///
/// Ported to C# by Simon McCorkindale <simon <at> aroha.mobi>

using UnityEngine;
using System.Collections;
/// <summary>
/// Gyroscope-controlled camera for iPhone  Android revised 2.26.12
/// </summary>
public class GyroCam : MonoBehaviour
{
	private bool gyroBool;
	private Gyroscope gyro;
	private Quaternion rotFix;

	private bool gyroAngleDetected = false;
	private Quaternion androidRotFix;
	private float angle = 0;


	public bool GyroAngleDetected 
	{
		get {
			return this.gyroAngleDetected;
		}
	}

	
	public void Start ()
	{
		Transform currentParent = transform.parent;
		GameObject camParent = new GameObject ("GyroCamParent");
		camParent.transform.position = transform.position;
		transform.parent = camParent.transform;
		GameObject camGrandparent = new GameObject ("GyroCamGrandParent");
		camGrandparent.transform.position = transform.position;
		camParent.transform.parent = camGrandparent.transform;
		camGrandparent.transform.parent = currentParent;
		
		#if UNITY_3_4
		gyroBool = Input.isGyroAvailable;
		#else
		gyroBool = SystemInfo.supportsGyroscope;
		#endif
		
		if (gyroBool) {
			
			gyro = Input.gyro;
			gyro.enabled = true;
			
			if (Screen.orientation == ScreenOrientation.LandscapeLeft) {
				camParent.transform.eulerAngles = new Vector3 (90, 180, 0);
			} else if (Screen.orientation == ScreenOrientation.Portrait) {
				camParent.transform.eulerAngles = new Vector3 (90, 180, 0);
			} else if (Screen.orientation == ScreenOrientation.PortraitUpsideDown) {
				camParent.transform.eulerAngles = new Vector3 (90, 180, 0);
			} else if (Screen.orientation == ScreenOrientation.LandscapeRight) {
				camParent.transform.eulerAngles = new Vector3 (90, 180, 0);
			} else {
				camParent.transform.eulerAngles = new Vector3 (90, 180, 0);
			}
			
			if (Screen.orientation == ScreenOrientation.LandscapeLeft) {
				rotFix = new Quaternion (0, 0, 1, 0);
			} else if (Screen.orientation == ScreenOrientation.Portrait) {
				rotFix = new Quaternion (0, 0, 1, 0);
			} else if (Screen.orientation == ScreenOrientation.PortraitUpsideDown) {
				rotFix = new Quaternion (0, 0, 1, 0);
			} else if (Screen.orientation == ScreenOrientation.LandscapeRight) {
				rotFix = new Quaternion (0, 0, 1, 0);
			} else {
				rotFix = new Quaternion (0, 0, 1, 0);
			}


			//Screen.sleepTimeout = 0;
		} else 
		{
			Debug.Log("No Gyro");
		}
	}
	

public void Update ()
	{
		if (gyroBool) {// & this.gameObject.camera.enabled
			{
				Quaternion quatMap;
				#if UNITY_IPHONE
				quatMap = gyro.attitude;
				#elif UNITY_ANDROID
				quatMap = new Quaternion(gyro.attitude.x,gyro.attitude.y,gyro.attitude.z,gyro.attitude.w);
//				androidRotFix = quatMap * rotFix;
//				angle  = Quaternion.Angle(transform.localRotation, androidRotFix);
//
//				if(angle > 1.5f)
//				{
//					this.gyroAngleDetected =  true;
//				}
//				else
//				{
//					this.gyroAngleDetected = false;
//				}

//				Debug.Log("Angle: " + gyroAngleDetected.ToString()+" " + angle.ToString());
				#endif
	
				transform.localRotation = quatMap*rotFix;
			}
		}
	}
//	
//	float scroll = 0f;
//	
//	public void OnGUI ()
//	{
//		if (gyroBool & this.gameObject.camera.enabled) {
//			// Add scrollbar to rotate around Y axis 0-360 for manual adjustment
////			scroll = GUI.VerticalScrollbar (new Rect (Screen.width - 35, 25, 30, Screen.height / 2), scroll, 1.0f, 0.0f, 360.0f);
//			scroll = GUI.VerticalScrollbar (new Rect (Screen.width - 35, 25, 30, Screen.height / 2), scroll, 0.1f, 0f, 0.9f);
//		}
////		if (GUI.changed) {
////			transform.parent.eulerAngles = new Vector3 (transform.parent.eulerAngles.x, scroll, transform.parent.eulerAngles.z);
////		}
//
//		string info = transform.position.ToString()+"  "+ transform.eulerAngles.ToString();
//
//		GUI.Label(new Rect(0,150,400,100), scroll.ToString());
//
//	}
}


