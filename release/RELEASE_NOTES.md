# 40 Winks: Recompiled v0.1.0

First public release of 40 Winks: Recompiled, a native Windows port of the N64 game made with N64: Recompiled and RT64.

**You need your own copy of the game**: the final N64 release by Piko Interactive (Europe, header `N4WZ`,
SHA-1 `282b97c6a6eb79535e618fc2651b6156fff65ea4`). This release contains no game assets.

> **State**: World 1 has been played from start to finish without bugs. Later worlds have not been tested yet.
> Keep a backup of your saves (`%LOCALAPPDATA%\40WinksRecompiled\saves`).

## Features
- High framerate (up to your monitor's refresh rate): camera, characters, objects, rain, particles, HUD and menus
  are interpolated. Gameplay is unchanged.
- Widescreen (Aspect Ratio Expand), 1 and 2 players, with the HUD on the screen edges.
- Full-screen 4:3 views in 1 player and 2-player split screen.
- Controller Pak saves (port 1).
- Film grain option, original 50 Hz timing (sound in sync with cutscenes).

## Install
1. Extract `40WinksRecompiled-v0.1.0-windows.zip`.
2. Run `40WinksRecompiled.exe` and select your ROM.

## Known issues
- Only World 1 is fully tested.
- Screen fades stay at 25 frames per second.
- Some launcher options are greyed out (sound volume, some graphics options, mods).

Please report bugs in the [issues](https://github.com/DreaXer/40WinksRecomp/issues) (version, where, what you did,
what happened, screenshot, and `40winks-log.txt` for a crash).

---

## Français

Première version publique : portage natif Windows du jeu N64, fait avec N64: Recompiled et RT64.
**Il faut ta propre copie du jeu** (version finale N64 de Piko Interactive, Europe, SHA-1 ci-dessus). Le monde 1 a été
joué en entier sans bug ; la suite n'est pas encore testée. Garde une copie de tes sauvegardes.
Installation : extraire le zip, lancer `40WinksRecompiled.exe`, choisir la ROM.
