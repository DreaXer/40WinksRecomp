40 WINKS : DREAMCOMPILED
Version 0.4.0
========================

A native Windows and Linux port of the N64 game 40 Winks, made with N64: Recompiled and RT64.
It does NOT contain any game asset: you need your own copy of the game.
Project page: https://github.com/DreaXer/40WinksRecomp

SUPPORTED ROM
-------------
Only the final N64 release by Piko Interactive (Europe):
  header code N4WZ, SHA-1 282b97c6a6eb79535e618fc2651b6156fff65ea4

HOW TO PLAY
-----------
1. Run 40WinksRecompiled.exe (Windows), or on Linux open a terminal in this folder and run
   ./40WinksRecompiled (start it from this folder: it needs assets/).
2. Select your 40 Winks ROM when asked (first launch only; a wrong dump is refused with the reason).
3. Start the game from the launcher.
Settings and saves are stored in %LOCALAPPDATA%\40WinksRecompiled\ (Windows)
or ~/.config/40WinksRecompiled/ (Linux).

Requirements: Windows 10/11 64-bit or Linux x86-64, a GPU with Direct3D 12 or Vulkan 1.2 (Vulkan on Linux),
a CPU with AVX. Linux also needs SDL2, GTK 3, FreeType, libcurl and the Vulkan loader
(Ubuntu/Debian: sudo apt install libsdl2-2.0-0 libgtk-3-0 libfreetype6 libcurl4 libvulkan1).
If the game crashes on startup, update your graphics drivers.

CURRENT STATE
-------------
Worlds 1 and 2 (with its boss) have been played from start to finish without bugs. Later worlds are not tested yet.
Keep a backup of your saves.

KNOWN ISSUES
------------
* Only Worlds 1 and 2 are fully tested.
* Screen fades stay at 25 frames per second.
* A few launcher options are greyed out (main volume, graphics API, mods).

REPORTING A BUG
---------------
https://github.com/DreaXer/40WinksRecomp/issues
Give: version, Windows version or Linux distribution, where it happened, what you did just before, what happened (screenshot if possible),
your Framerate and Aspect Ratio settings, and for a crash the 40winks-log.txt file next to the executable
(on Linux, in the folder you started it from).

LICENSE
-------
GPL-3.0 (see LICENSE). 40 Winks is the property of its respective owners; this project is not affiliated with them.
