# ggxrd_replay_mod

## Description

This mod for Guilty Gear Xrd Rev 2 version 2211 works as of 30'th December 2023. It allows you to record some game footage with players' inputs into a file, then open said file and copy inputs from it as text for the GGXrd wakeup (reversal) tool. What will help you find the inputs is being able to play/seek/scrub the video footage along with the inputs.

## Credits

Thanks to WorseThanYou for finding the input ring buffer, from which the inputs data is taken, and for finding many other things that are used extensively in all my mods.  
Thanks to GGXrd Reversal Tool for information about the input data format (https://github.com/Iquis/rev2-wakeup-tool).  
Thanks to Altimor's GGXrd Hitbox overlay mod for the original hitbox-drawing code (http://www.dustloop.com/forums/index.php?/forums/topic/12495-xrd-pc-hitbox-overlay-mod/).  
Thanks to OBS studio for an idea of how game video capture could be implemented (https://obsproject.com/).

## System requirements

Windows 10. Could maybe work on 7, I haven't tested it. Will definitely not work on Linux running GGXrd under Steam Proton, not even with Wine.  
The app benefits greatly from having multiple CPU cores. The impact of having a video card has not been tested.

Only Guilty Gear Xrd Rev 2 version 2211 has been tested to work. A newer version might not work.

## Quickstart

Video guide: https://youtu.be/HR6x9U7tNlw

Steps:

1. Start Guilty Gear Xrd Rev 2.
2. Start `ggxrd_replay_viewer.exe`. You can press Record now or later.
3. Start a match. At any point during the match, you can start recording.
4. When the match finishes, the recording stops automatically (you only see it when you press Stop recording). You can stop recording manually at any moment.
5. In `ggxrd_replay_viewer` go to File - Open or press Ctrl+O to open the file you just recorded.
6. Select a section of the inputs you want to copy by clicking and dragging with the mouse.
7. Press Ctrl+C to show a dialog box with the inputs displayed as text that you can copy.
8. Paste the inputs into the reversal tool. You have to go into training mode with the right characters, etc yourself, this is not automated.

You cannot play the video while recording and you cannot record a video while playing it.

## Controls

The app window has several sections:

* The button bar:
  * Record - starts or stops the recording. Recording will stop the playback and actually close the old file.
  * Play - plays or pauses the video. You cannot play the video while recording.
  * Step one frame backwards (hotkey , or <, can be held)
  * Step one frame forward (hotkey . or >, can be held)
* Left and right panels containing inputs (to see inputs you need to go to File - Open and open a file first, even if you just finished recording a file):
  * Follow button - toggles the follow mode, during which the panel automatically scrolls to the last input corresponding to the position in the video. You cannot scroll the panel manually during follow mode, unless the video is paused.
  * The inputs themselves - you can select the inputs using: Mouse click, Shift + Mouse click, Mouse drag, Shift + Mouse drag, (optionally Shift+) Up/Down arrow keys, (optionally Shift+) Page up/down keys, (optionally Shift+) Home/end keys. The selected inputs range can only be contiguous.
  * The left edge of the panel - can be used as a seekbar. By dragging it with your mouse you can seek to the portion of the video that corresponds to that input. This might work odd in recorded online matches affected by rollback.
  * Mouse wheel and scrollbars - used to scroll the panels.
* Video
  * Clicking the video itself does nothing
  * The seekbar under the video - can be used to navigate the video
* Left/Right arrow keys - can be used to go backwards or forward 30 logical frames. Can be held.
* Settings menu - invoke the settings menu by going into the top menubar - Edit - Settings
  * Display hitboxes in-game - check the box and press OK to display hitboxes in the game (game must be launched by the time you press OK). Will not display boxes for invisible Chipps while participating in online matches
  * Display inputs in-game - check the box and press OK to display inputs in the game. Will not display inputs when participating in online matches, but will if observing.
  * Record every N'th frame - this regulates how many visual frames are recorded per second. Logical frames (inputs) do not get skipped. This is mainly used to conserve file size. And yes, you can set it to record every frame to get 60 FPS, provided your machine can handle that. Visual frames can be skipped if the encoder is not fast enough to encode all of them.

## Project dependencies

### libpng

Used to load PNG icon resources. This gets included in the project and does not require installation.

### Microsoft Detours

Used to hook functions in the dll part of the mod. This gets included in the project and does not require installation.

### DirectX SDK

DirectX is used in the dll part of the mod to draw inputs and hitboxes on the screen. It's also used in the app part to convert YUY2 video format into XRGB when playing the video. It's not used when recording a video, unless the Microsoft H.264 codec is doing something hardware-assisted (but I don't know how it works).  
The user must have d3d9.dll on their system, and it's not shipped with the project.  

### zlib

Is needed by libpng. This gets included in the project and does not require installation.

### Microsoft.Windows.Common-Controls version 6.0.0.0

Is needed to draw the scrollbar and the tooltips.  
This library must be already installed on the user's system or else there may be errors. It's not shipped with the project.

### Microsoft H.264 codec

I don't know if this is needed to build but it definitely must be installed on the user's machine or else the whole app will not even start.  
This is used to encode and decode the video. It is probably part of Windows Media Player and/or Microsoft Media Foundation Platform.

## How to build

The idea is to build the `ggxrd_replay_viewer` as 64-bit, while the hook must be always 32-bit, because the game is 32-bit. Use Visual Studio 2022 Community Edition to build.  
Go to `ggxrd_replay_viewer` project settings in the Solution Explorer and navigate to Advanced.  
Set Character Set - Use Unicode Character Set.  
Navigate to VC++ Directories.  
Add following paths to Include Directories: `path_to_your_directx_9_sdk\Include;path_to_your_libpng-libpng16;$(IncludePath)`  
Add following paths to Library Directories: `path_to_your_directx_9_sdk\Lib\x64;path_to_your_libpng-libpng16\projects\vstudio\x64\Release Library;$(LibraryPath)`  
Navigate to Linker - Input.  
Add following libraries to Additional Dependencies: `libpng16.lib;zlib.lib;Rpcrt4.lib;Strmiids.lib;d3d9.lib;%(AdditionalDependencies)`  
Rpcrt4.lib is needed for UuidCompare (I actually don't need this function anymore, but forgot to remove it).  
Strmiids.lib is needed for IID_ICodecAPI definition. These should be in the Windows SDK installed by Visual Studio.

## GG btns.fla

This is a Flash 8 Professional file. It can probably be opened by Adobe Animate, Adobe Flash CS3/4/5/6. It contains the art for the buttons used in the dll and app portions of the mod. You don't need this file unless you want to edit the button visuals.
