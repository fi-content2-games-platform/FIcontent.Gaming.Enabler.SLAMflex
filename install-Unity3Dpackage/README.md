SLAMflex v0.1.0 - 2015-01-30
============================
Install guide
----------------
- Open new project in Unity3D editor and switch platform to android.
- Import SLAMflex.unitypackage.
- Open SLAMflex\Scenes\main scene.
- Import or create GameObject and add it as a child of GroundPlane GameObject in the hierarchy of main scene. After plane is detected your GameObject will be enabled.
- Open SLAMflex\Scripts\SlamFlexAdapter.cs and start by redefining   
 - void OnGUI() : starting and stoping plane detection
  
For documentation please refer to the FIcontent Wiki at http://wiki.mediafi.org in particular
the documentation of the Pervasive Games Platform at http://wiki.mediafi.org/doku.php/ficontent.gaming.architecture
