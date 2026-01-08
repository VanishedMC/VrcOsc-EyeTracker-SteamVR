# VRChat eyetracking OSC to SteamVR 

This software enables the use of [XR_EXT_eye_gaze_interaction](https://registry.khronos.org/OpenXR/specs/1.0/html/xrspec.html#XR_EXT_eye_gaze_interaction) driven by [vrchats OSC eye tracking data](https://docs.vrchat.com/docs/osc-eye-tracking) 

DISCLAIMER: This software is distributed as-is, without any warranties or conditions of any kind. Use at your own risks.

This mainly allows using the ET gaze data with DFR.  
The intended usecase is combining [Baballonia](https://github.com/Project-Babble/Baballonia) gaze tracking with DFR enabled games such as iRacing or [DCS with quadviews](https://github.com/mbucchia/Quad-Views-Foveated) on a bigscreen beyond.  
Any other application or headset providing vrchat with ET can be used as well

# Setup

Download [the latest release](https://github.com/VanishedMC/VrcOsc-EyeTracker-SteamVR/releases)

- Unzip in a folder that will be permanent (ie: that you will not move or be renamed).  
- SHUT DOWN STEAMVR ENTIRELY  
- Run the Register-Driver.bat script.  
- Configure babble to send data to port `9020`
- Enable `Use Native VRC Eye Tracking`
- Optionally, disable the VRCFT module entirely to decrease OSC traffic
  
<img width="500" alt="Baballonia Desktop_BfcUvdCaSQ" src="https://github.com/user-attachments/assets/963e39a1-70cd-4da6-8473-63067373e0ab" />
<img width="500" alt="Baballonia Desktop_e6rGbfx1Aw" src="https://github.com/user-attachments/assets/4723652f-b285-4b03-8e1f-a7e453b8b177" />


# Credits
This program is a combination of MBucchia's [Pimax-EyeTracker-SteamVR](https://github.com/mbucchia/Pimax-EyeTracker-SteamVR) and RidgeVR's [OpenXR-Eye-Trackers-VRC](https://github.com/RidgeXR/OpenXR-Eye-Trackers-VRC/). MIT license applies
