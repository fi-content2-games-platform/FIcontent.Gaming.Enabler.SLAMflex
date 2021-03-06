#define TRANSLATION
//#define GYRO
//#define GYRO_WITH_TRANSLATION
//#define NO_GYRO
//#define NO_GYRO_WITH_ROT
//#define CORNER_TEX

using UnityEngine;
using System;
using System.Collections;
using System.IO;
using System.Collections.Generic;
using System.Runtime.InteropServices;


/// <summary>
/// SlamFlexAdapter is example how setup SLAMflex library, setup image data from webtexture and every frame sending it to plugin. Also setup for callback when plugin sends data to Unity3D 
/// </summary>
public class SlamFlexAdapter : MonoBehaviour
{
	private GameObject _bg_panel;
	private GameObject _bg_panel_corner_points;

	protected Color32[] _buf;
	private WebCamTexture w;
	private GCHandle m_PixelsHandle;


	public GameObject _ground_plane;
	private int screen_width = 640;
	private int screen_height = 480;
	private float aspect = 1.38188006064344f;
	private bool autoend = false;
	private Vector3 orginalPlainePosition;

	private bool textureOk = false;
	private bool planeRotationFound = false;
	private Vector3 planeRotation, planeTranslation = Vector3.zero;
	private string userMessagesTextInstr = "Translate the camera slowly sideways";
	private string userMessagesText = "";
	private string userMessagesText1 = "";

	private Texture2D cornerPoints;
	private GyroCam gyroCam;

	private string logFileData = "";
	public bool enable_logging = false;

	private SlamFlexWrapper.DetectionState current_State = SlamFlexWrapper.DetectionState.Finished;


	private SlamFlexWrapper.SendStringDelegate mSendStringDelegate;
	private SlamFlexWrapper.SendPoseDelegate mSendPoseDelegate;
	private SlamFlexWrapper.SendLogDelegate mSendLogDelegate;
	private SlamFlexWrapper.SendArrayPointsDelegate mSendArrayPointsDelegate;

    /// <summary>
    /// Function for unity3D code initialization
    /// </summary>
	void Awake()
	{
		//setup unity3D webcam
		WebCamDevice[] devices= WebCamTexture.devices;
		if (devices.Length > 0)
		{
			w=new WebCamTexture(screen_width, screen_height, 15);

			//setup background
			camera.aspect = aspect;
			this._bg_panel=GameObject.Find("Plane");
			this._bg_panel.renderer.material.mainTexture=w;
			w.Play();

#if CORNER_TEX
			//setup texture for rendering corner points on separate texture
			this._bg_panel_corner_points = GameObject.Find ("TrackingCornerPoints");

			cornerPoints = new Texture2D(screen_width, screen_height);
			Color[] data = new Color[screen_width*screen_height];
			Color c = new Color(255, 255, 255, 0);
			for (int i = 0; i<data.Length; i++)
			{
				data[i] = c;
			}

			cornerPoints.SetPixels(data);
			cornerPoints.Apply();

			this._bg_panel_corner_points.renderer.material.mainTexture = cornerPoints;
#endif
	
		}
		else
		{
			Debug.LogError("No Webcam.");
		}

#if GYRO || GYRO_WITH_TRANSLATION
		gyroCam = transform.GetComponent<GyroCam>();
		gyroCam.enabled = true;
#endif
	}


    /// <summary>
    /// Function for unity3D code initialization
    /// </summary>
	void Start ()
	{
		//Setup logger
		//load previous settings
		if (enable_logging)
		{
			StreamReader fileReader = null;
			string logFile = Application.persistentDataPath + "/log.txt";


			if (File.Exists(logFile))
			{
				fileReader = new StreamReader(logFile);
				
				logFileData = fileReader.ReadToEnd();
				fileReader.Close();
			}
		}

		//first few frames is not valid, have to wait for valid frame
		StartCoroutine (CheckTheValidityOfTexture());

		//implement SlamFlex hooks 
		mSendStringDelegate = new SlamFlexWrapper.SendStringDelegate(ReciveStringFromPlugin);
		mSendPoseDelegate = new SlamFlexWrapper.SendPoseDelegate(RecivePoseFromPlugin);
		mSendLogDelegate = new SlamFlexWrapper.SendLogDelegate(Logging);
		mSendArrayPointsDelegate = new SlamFlexWrapper.SendArrayPointsDelegate(ReciveArrayPointsFromPlugin);

		//initialize SLAMflex plugin
		SlamFlexWrapper.StartSlam (mSendStringDelegate, mSendPoseDelegate, mSendLogDelegate, mSendArrayPointsDelegate);


	}



	void Update ()
	{
		//we have valid texture as input, each update send one frame to plugin
		if (textureOk && !planeRotationFound) 
		{
			this._buf = (Color32[])this.w.GetPixels32 ();
			m_PixelsHandle = GCHandle.Alloc (this._buf, GCHandleType.Pinned);
			current_State = SlamFlexWrapper.SetNewFrame (m_PixelsHandle.AddrOfPinnedObject (), this.w.width, this.w.height);
			m_PixelsHandle.Free ();
		}
	}


    /// <summary>
    /// Function for yield of unity3D update until webtexture is initialized
    /// </summary>
    /// <returns></returns>
	private IEnumerator CheckTheValidityOfTexture()
	{
		while (true) 
		{
			if (w.height < 100)
			{
				Debug.Log("WebTexture not initialized");
				yield return null;
			}
			else
			{
				textureOk = true;
				break;
			}
		}
	}



    /// <summary>
    /// Function for communication between plugin and Unity, plugin sends plain string
    /// </summary>
    /// <param name="message">string message from plugin</param>
	private void ReciveStringFromPlugin(string message)
	{

		if(message == "Tracker_ResetMM")
		{
			StopAllCoroutines();
			StartCoroutine(AutoEnd());
		}
		else if(message.StartsWith("Corners"))
		{
			userMessagesText = message;
		}
		else
		{
			Debug.Log("Message from plugin:  " + message);
		}
	}


    /// <summary>
    /// Function for communication between plugin and Unity, plugin sends array of 
	/// points (corners) to Unity
    /// </summary>
    /// <param name="pointer">pointer of array to copy from unmanaged to managad code</param>
    /// <param name="size">size of array</param>
	private void ReciveArrayPointsFromPlugin(IntPtr pointer, int size )
	{
		int [] corners = new int[size];
		Marshal.Copy(pointer, corners, 0, size);

		SetCornerTexturePoints(corners);
	}


    /// <summary>
    /// Function for drawing corners on separate texture	
    /// </summary>
    /// <param name="corners">array of corners for draw</param>
	private void SetCornerTexturePoints(int[] corners)
	{

		Color[] data = new Color[screen_width*screen_height];
		Color c = new Color(255, 255, 255, 0);
		for (int i = 0; i<data.Length; i++)
		{
			data[i] = c;
		}
	
		for(int i = 0; i < corners.Length; i++)
		{
			data[corners[i]] = Color.red;
		}

		cornerPoints.SetPixels(data);
		cornerPoints.Apply();
	}

    /// <summary>
    /// Function for reciving pose from plugin, rotation in euler angles and translation
    /// </summary>
    /// <param name="r1">rotation euler angle for unity game object transform X</param>
    /// <param name="r2">rotation euler angle for unity game object transform Y</param>
    /// <param name="r3">rotation euler angle for unity game object transform Z</param>
    /// <param name="t1">translation for unity game object transform X</param>
    /// <param name="t2">translation for unity game object transform Y</param>
    /// <param name="t3">translation for unity game object transform Z</param>
	private void RecivePoseFromPlugin (float r1, float r2, float r3, double t1, double t2, double t3)
	{
		
		planeRotation.x = r1;
		planeRotation.y = r2;
		planeRotation.z = r3;
		
		planeTranslation.x = (float)t2;
		planeTranslation.y = (float)t1;
//		planeTranslation.z = (float)t3; for later
		

		if(autoend)
		{
			//Debug.Log("Pose:  "+r1.ToString()+" "+r2.ToString()+" "+r3.ToString()+" "+t1.ToString()+" "+t2.ToString()+" "+t3.ToString());
			this._ground_plane.SetActive (true);
			this._ground_plane.transform.eulerAngles = planeRotation + this._bg_panel.transform.eulerAngles;



#if GYRO || GYRO_WITH_TRANSLATION

			Vector3 planePosition = this._bg_panel.transform.position;

			if (planePosition.x > 0) 
				planePosition.x -= 1000f;
			else
				planePosition.x += 1000f;
			
			if (planePosition.y > 0) 
				planePosition.y -= 1000f;
			else
				planePosition.y += 1000f;
			
			if (planePosition.z > 0) 
				planePosition.z -= 1000f;
			else
				planePosition.z += 1000f;
#endif
			

			Debug.Log ("Unity_ Plane found");
	//		SlamFlexWrapper.Stop ();

			StopAllCoroutines();

#if GYRO || GYRO_WITH_TRANSLATION
			this._ground_plane.transform.position = planePosition;
			orginalPlainePosition = planePosition;
#else

			orginalPlainePosition = this._ground_plane.transform.position;
#endif
//		planeRotationFound = true;
		}
		autoend = false;

#if GYRO_WITH_TRANSLATION
		planeTranslation.x *= 4140f;
		planeTranslation.y *= -3000f;
		if(!gyroCam.GyroAngleDetected)
		{
			this._ground_plane.transform.position = orginalPlainePosition + planeTranslation;
		}
#endif

#if TRANSLATION
		float offset = 1;
		if((float)t3 < 1.75f)
		{
			offset = 2;
		}
		planeTranslation.x *= 4140f * offset;
		planeTranslation.y *= -3000f * offset;
		this._ground_plane.transform.position = orginalPlainePosition + planeTranslation;

#endif

#if NO_GYRO_WITH_ROT
		this._ground_plane.transform.eulerAngles = planeRotation + this._bg_panel.transform.eulerAngles;
#endif
		//userMessagesText1 = "t1: " + t1.ToString()+"    t2: " + t2.ToString() + "\nt3: " + t3.ToString();


	}
	

    /// <summary>
    /// Function for starting SLAMflex until enough points for generation of map is found
    /// </summary>
    /// <returns></returns>
	private IEnumerator AutoEnd()
	{
		autoend = true;
		while (autoend)
		{
			yield return new WaitForSeconds(1f);
			SlamFlexWrapper.StartPlaneDetection();
			Debug.Log ("AutoEnd finished");
		}

	}


    /// <summary>
    /// Function for logging data on Application.persistentDataPath + "/log.txt"
    /// </summary>
    /// <param name="message"> message to log</param>
	private void Logging (string message)
	{
		if(enable_logging)
		{
			logFileData += message;
		}
	}



    /// <summary>
    /// Function for presentation of user messages on GUI
    /// </summary>
    /// <param name="message">message to present</param>
	public void UserMessages (string message)
	{
		userMessagesText = message;
	}


    /// <summary>
    /// Function for unity3D GUI
    /// </summary>
	void OnGUI()
	{
		if(	GUI.Button(new Rect(Screen.width*0.9f, 0, Screen.width*0.1f, Screen.width*0.1f), "Exit"))
		{
			Debug.Log("Unity_ App exits");
			Application.Quit();
		}
		
		if(	GUI.Button(new Rect(Screen.width - (Screen.width*0.3f),(Screen.height-(Screen.width*0.3f)),Screen.width*0.3f,Screen.width*0.3f), "Start Plane Detection"))
		{
			Debug.Log("Unity_ Tracking started");
			planeRotationFound = false;
			this._ground_plane.SetActive(false);
			Camera.main.transform.eulerAngles = Vector3.zero;
			SlamFlexWrapper.StartPlaneDetection();
			StartCoroutine(AutoEnd());
		}

		if(	GUI.Button(new Rect(0,(Screen.height-(Screen.width*0.3f)),Screen.width*0.3f,Screen.width*0.3f), "Stop Plane Detection"))
		{
			Debug.Log("Unity_ Tracking stoped");
			planeRotationFound = false;
			this._ground_plane.transform.position = orginalPlainePosition;
			this._ground_plane.SetActive(false);
			Camera.main.transform.eulerAngles = Vector3.zero;
			SlamFlexWrapper.StopSlam();
		}

		string text  = "";
		if(autoend)
		{
			text  = "\nMessages: "+userMessagesTextInstr+"\n"+userMessagesText+"\n" + userMessagesText1;
		}
		else
		{
			text  = "\nMessages: "+userMessagesText+"\n" + userMessagesText1;
		}


		GUI.Label (new Rect (0f, 0f, 200, 200), "Current tracking state: " + current_State.ToString());
		GUI.Label (new Rect (0, 100, 400, 400), text);
	}


    /// <summary>
    /// Function for unity3D OnApplicationQuit event
    /// </summary>
	void OnApplicationQuit()
	{
		if(enable_logging)
		{
			string logFile = Application.persistentDataPath + "/log.txt";
			var sr = File.CreateText(logFile);
			sr.WriteLine (logFileData);
			sr.Flush();
			sr.Close();
		}
	}



	private List<Vector2> ParseCoordinates(string array)
	{
		
		List<Vector2> ret = new List<Vector2>();
		
		string[] coordinates = array.Split(new char[]{';'});
		
		foreach(string s in coordinates)
		{
			string[] coor = s.Split(new char[]{','});
			if(coor.Length == 2)
			{
				int x = Convert.ToInt32(coor[0]);
				int y = Convert.ToInt32(coor[1]);
				if(x<screen_width+1 && x>0 && y<screen_height+1 && y>0)
				{
					ret.Add(new Vector2(x, y));
				}
				
			}
		}
		
		return ret;
	}

}
