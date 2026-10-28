40 WINKS : DREAMCOMPILED
Version 0.1.1
========================

A native Windows port of the N64 game 40 Winks, made with N64: Recompiled and RT64.
It does NOT contain any game asset: you need your own copy of the game.
Project page: https://github.com/DreaXer/40WinksRecomp

SUPPORTED ROM
-------------
Only the final N64 release by Piko Interactive (Europe):
  header code N4WZ, SHA-1 282b97c6a6eb79535e618fc2651b6156fff65ea4

HOW TO PLAY
-----------
1. Run 40WinksRecompiled.exe.
2. Select your 40 Winks ROM when asked.
3. Start the game from the launcher.
Settings and saves are stored in %LOCALAPPDATA%\40WinksRecompiled\.

Requirements: Windows 10/11 64-bit, a GPU with Direct3D 12 or Vulkan 1.2, a CPU with AVX.
If the game crashes on startup, update your graphics drivers.

CURRENT STATE
-------------
World 1 has been played from start to finish without bugs. Later worlds are not tested yet.
Keep a backup of your saves.

KNOWN ISSUES
------------
* Only World 1 is fully tested.
* Screen fades stay at 25 frames per second.
* Some launcher options are greyed out (sound volume, some graphics options, mods).

REPORTING A BUG
---------------
https://github.com/DreaXer/40WinksRecomp/issues
Give: version, Windows version, where it happened, what you did just before, what happened (screenshot if possible),
your Framerate and Aspect Ratio settings, and for a crash the 40winks-log.txt file next to the executable.

LICENSE
-------
GPL-3.0 (see LICENSE). 40 Winks is the property of its respective owners; this project is not affiliated with them.
