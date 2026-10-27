# 🌙 40 Winks : Dreamcompiled — v0.1.0

First public release of **40 Winks : Dreamcompiled**, a native Windows port of the Nintendo 64 game, made by statically
recompiling its code with [N64: Recompiled](https://github.com/N64Recomp/N64Recomp) and rendered with
[RT64](https://github.com/rt64/rt64).

> [!IMPORTANT]
> **You need your own copy of the game.** This release contains no game assets.
>
> | Supported ROM | |
> |---|---|
> | Release | Final N64 release by Piko Interactive (Europe) |
> | Header code | `N4WZ` |
> | SHA-1 | `282b97c6a6eb79535e618fc2651b6156fff65ea4` |

> [!WARNING]
> **Early release.** World 1 has been played from start to finish without bugs; later worlds have not been tested yet.
> Keep a backup of your saves: `%LOCALAPPDATA%\40WinksRecompiled\saves`.

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

1. Download **`40WinksRecompiled-v0.1.0-windows.zip`** below and extract it anywhere.
2. Run **`40WinksRecompiled.exe`** and select your ROM.
3. Start the game from the launcher.

**Requirements:** Windows 10/11 (64-bit) · GPU with Direct3D 12 or Vulkan 1.2 · CPU with AVX.
If the game crashes on startup, update your graphics drivers first.

## 🐛 Known issues

- Only World 1 is fully tested.
- Screen fades stay at 25 frames per second.
- Some launcher options are greyed out (sound volume, some graphics options, mods).

## 📣 Reporting a bug

Open an [issue](https://github.com/DreaXer/40WinksRecomp/issues) with:

- the version (`0.1.0`) and your Windows version;
- where it happened and what you did just before;
- what happened, with a screenshot if possible;
- your **Framerate** and **Aspect Ratio** settings;
- for a crash, the `40winks-log.txt` file next to the executable.

<details>
<summary>🇫🇷 <b>En français</b></summary>

<br>

Première version publique de **40 Winks : Dreamcompiled**, portage natif Windows du jeu N64, fait avec N64: Recompiled
et RT64.

- **Il faut ta propre copie du jeu** : version finale N64 de Piko Interactive (Europe), code `N4WZ`,
  SHA-1 `282b97c6a6eb79535e618fc2651b6156fff65ea4`. Cette release ne contient aucune donnée du jeu.
- **Version de test** : le monde 1 a été joué en entier sans bug ; la suite n'est pas encore testée. Garde une copie
  de tes sauvegardes (`%LOCALAPPDATA%\40WinksRecompiled\saves`).
- **Installation** : extraire le zip, lancer `40WinksRecompiled.exe`, choisir la ROM.
- **Bugs** : ouvre une [issue](https://github.com/DreaXer/40WinksRecomp/issues) (version, endroit, ce que tu
  faisais, capture, réglages Framerate / Aspect Ratio, et `40winks-log.txt` pour un plantage).

</details>

---

<sub>40 Winks is the property of its respective owners. This project is not affiliated with them. Licensed under GPL-3.0.</sub>
