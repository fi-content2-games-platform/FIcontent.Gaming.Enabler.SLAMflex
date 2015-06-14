# SLAMflex SE install instructions

### Download Unity
If Unity is not installed, follow this link to get the latest version of Unity:

[https://unity3d.com/get-unity/download](https://unity3d.com/get-unity/download)

Note that Unity takes a long time to install.

### Clone the repository
Clone this repository with the following command:

```
git clone https://github.com/fi-content2-games-platform/FIcontent.Gaming.Enabler.SLAMflex.git
```

You need to git to do so, help on how to install it is available at https://help.github.com/articles/set-up-git

### Import to Unity
Open new project in Unity3D editor and switch platform to android. Import SLAMflex.unitypackage:
```
FIcontent.Gaming.Enabler.SLAMflex\install-Unity3Dpackage\SLAMflex.unitypackage
```
Open the scene:
```
your Unity project folder\Assets\SLAMflex\Scenes\main.unity
```
Import or create GameObject and add it as a child of GroundPlane GameObject in the hierarchy of main scene. After plane is detected your GameObject will be enabled.

Open 
```
your Unity project folder\Assets\SLAMflex\Scripts\SlamFlexAdapter.cs
```
and start by redefining

    void OnGUI() : starting and stoping plane detection

###Build and Run project on android phone
Point to keyboard for example and press Start Plane Detection button. After that move phone horizontally across keyboard. Plane detection will be started multiple times until plane is detected.


For documentation please refer to the FIcontent Wiki at http://wiki.mediafi.org in particular the documentation of the Pervasive Games Platform at http://wiki.mediafi.org/doku.php/ficontent.gaming.architecture
