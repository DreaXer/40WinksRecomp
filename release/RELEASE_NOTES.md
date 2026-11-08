# 🌙 40 Winks : Dreamcompiled — v0.4.0

**Now on Linux too.** A native Windows and Linux port of the Nintendo 64 game, made by statically recompiling its
code with [N64: Recompiled](https://github.com/N64Recomp/N64Recomp) and rendered with [RT64](https://github.com/rt64/rt64).

> [!IMPORTANT]
> **You need your own copy of the game.** This release contains no game assets.
>
> | Supported ROM | |
> |---|---|
> | Release | Final N64 release by Piko Interactive (Europe) |
> | Header code | `N4WZ` |
> | SHA-1 | `282b97c6a6eb79535e618fc2651b6156fff65ea4` |

> [!WARNING]
> **Early release.** Worlds 1 and 2 (with its boss) have been played from start to finish without bugs; later worlds
> have not been tested yet. Keep a backup of your saves: `%LOCALAPPDATA%\40WinksRecompiled\saves`
> (Windows) or `~/.config/40WinksRecompiled/saves` (Linux).

## 🆕 What's new since v0.3.0

- **Linux build** (x86-64, Vulkan): `40WinksRecompiled-v0.4.0-linux.tar.gz`, with the same features as Windows,
  including the game-styled launcher. Tested on Ubuntu with X11 and an NVIDIA GPU.
- Linux settings, ROM and saves are stored in `~/.config/40WinksRecompiled/`.

## ✨ Features

| | |
|---|---|
| 🎞️ **High framerate** | Up to your monitor's refresh rate. Camera, characters, objects, rain, particles, HUD and menus are interpolated. Gameplay is unchanged. |
| 🖥️ **Widescreen** | Aspect Ratio *Expand* shows more of the world in 1 and 2 players, with the HUD on the screen edges. |
| 🔲 **Full-screen 4:3** | The 1-player view and both 2-player halves fill the whole picture. |
| 💾 **Controller Pak saves** | Save and load like on the console (port 1). |
| 👥 **2 players** | Split screen with two controllers. |
| 📺 **Film grain** | Optional, like the original console output. |
| 🔊 **Original timing** | 50 Hz, sound in sync with the cutscenes. |

## 📦 Install

1. Download **`40WinksRecompiled-v0.4.0-windows.zip`** or **`40WinksRecompiled-v0.4.0-linux.tar.gz`** below and
   extract it anywhere.
2. Windows: run **`40WinksRecompiled.exe`**. Linux: open a terminal in the extracted folder and run
   **`./40WinksRecompiled`** (start it from that folder: it needs `assets/`).
3. Select your ROM when asked, then start the game from the launcher.

**Requirements:** Windows 10/11 (64-bit) or Linux x86-64 · GPU with Direct3D 12 or Vulkan 1.2 (Vulkan on Linux) ·
CPU with AVX. Linux also needs SDL2, GTK 3, FreeType, libcurl and the Vulkan loader; on Ubuntu / Debian:
`sudo apt install libsdl2-2.0-0 libgtk-3-0 libfreetype6 libcurl4 libvulkan1`.
If the game crashes on startup, update your graphics drivers first.

## 🐛 Known issues

- Only Worlds 1 and 2 are fully tested.
- Screen fades stay at 25 frames per second.
- A few launcher options are greyed out (main volume, graphics API, mods).

## 📣 Reporting a bug

Open an [issue](https://github.com/DreaXer/40WinksRecomp/issues) with:

- the version (`0.4.0`) and your Windows version or Linux distribution;
- where it happened and what you did just before;
- what happened, with a screenshot if possible;
- your **Framerate** and **Aspect Ratio** settings;
- for a crash, the `40winks-log.txt` file next to the executable (on Linux, in the folder you started it from).

<details>
<summary>🇫🇷 <b>En français</b></summary>

<br>

Version 0.4.0 de **40 Winks : Dreamcompiled**, portage natif Windows et Linux du jeu N64, fait avec N64: Recompiled et RT64.

- **Nouveau** : version Linux (x86-64, Vulkan), avec les mêmes fonctions que sous Windows, launcher aux couleurs du
  jeu compris. Testée sous Ubuntu (X11, carte NVIDIA). Réglages, ROM et sauvegardes dans `~/.config/40WinksRecompiled/`.

- **Il faut ta propre copie du jeu** : version finale N64 de Piko Interactive (Europe), code `N4WZ`,
  SHA-1 `282b97c6a6eb79535e618fc2651b6156fff65ea4`. Cette release ne contient aucune donnée du jeu.
- **Version de test** : les mondes 1 et 2 ont été joués en entier sans bug ; la suite n'est pas encore testée. Garde une copie
  de tes sauvegardes (`%LOCALAPPDATA%\40WinksRecompiled\saves` ou `~/.config/40WinksRecompiled/saves`).
- **Installation** : extraire le zip (Windows) ou le tar.gz (Linux), lancer `40WinksRecompiled.exe` ou, sous Linux,
  `./40WinksRecompiled` depuis le dossier extrait, puis choisir la ROM quand c'est demandé. Linux demande aussi
  `sudo apt install libsdl2-2.0-0 libgtk-3-0 libfreetype6 libcurl4 libvulkan1` (Ubuntu / Debian).
- **Bugs** : ouvre une [issue](https://github.com/DreaXer/40WinksRecomp/issues) (version, endroit, ce que tu
  faisais, capture, réglages Framerate / Aspect Ratio, et `40winks-log.txt` pour un plantage).

</details>

---

<sub>40 Winks is the property of its respective owners. This project is not affiliated with them. Licensed under GPL-3.0.</sub>
