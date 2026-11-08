<p align="center">
  <img src="icon.png" alt="40 Winks : Dreamcompiled" width="256">
</p>

# 🌙 40 Winks : Dreamcompiled

**A native PC port of the Nintendo 64 game 40 Winks**, for Windows and Linux, made by **statically recompiling** the
game's code with [N64: Recompiled](https://github.com/N64Recomp/N64Recomp) and rendered with
[RT64](https://github.com/rt64/rt64), with enhancements such as high framerates and widescreen.

### 📥 [Download the latest release here](https://github.com/DreaXer/40WinksRecomp/releases)

> [!IMPORTANT]
> **You need your own copy of the game.** This repository and its releases contain no game assets, to play or to
> build this project.
>
> | Supported ROM | |
> |---|---|
> | Release | Final N64 release by Piko Interactive (Europe) |
> | Header code | `N4WZ` |
> | SHA-1 | `282b97c6a6eb79535e618fc2651b6156fff65ea4` |
>
> The earlier prototype (`N4WX`) is not supported.

> [!WARNING]
> **Version 0.4.0 is an early release.** Worlds 1 and 2 (with its boss) have been played from start to finish without
> bugs; later worlds have not been tested yet: expect problems there. Keep a backup of your saves:
> `%LOCALAPPDATA%\40WinksRecompiled\saves` (Windows) or `~/.config/40WinksRecompiled/saves` (Linux).

## ✨ Features

| | |
|---|---|
| 🎞️ **High framerate** | The game runs at 25 frames per second; RT64 draws extra frames in between, up to your monitor's refresh rate (Graphics > Framerate). Camera, characters, objects, rain, particles, HUD and menus are interpolated. Gameplay is unchanged. |
| 🖥️ **Widescreen** | Aspect Ratio *Expand* (default) shows more of the world in 16:9, in 1 and 2 players. The HUD can stick to the screen edges (HUD Placement). |
| 🔲 **Full-screen 4:3** | The 1-player view fills the whole 320×240 picture, and both 2-player halves are full height. |
| 💾 **Controller Pak saves** | Save and load like on the console (Controller Pak emulated on port 1). |
| 👥 **2 players** | Split screen with two controllers. |
| 📺 **Film grain** | Optional (Graphics tab), like the original console output. |
| 🔊 **Original timing** | The game runs at its original 50 Hz: correct sound speed, sound in sync with the cutscenes. |
| 🔤 **Game-styled launcher** | Menus, tabs, option names and sliders use the game's own menu letters and animations, read from your ROM at startup (nothing from the game is shipped). |
| ⚙️ **Extra graphics options** | MSAA, downsampling, high precision framebuffer, texture filtering, output filter, 2D upscale. |

## 📦 Install

1. Download **`40WinksRecompiled-v0.4.0-windows.zip`** or **`40WinksRecompiled-v0.4.0-linux.tar.gz`** from the
   [releases](https://github.com/DreaXer/40WinksRecomp/releases) and extract it anywhere.
2. Windows: run **`40WinksRecompiled.exe`**. Linux: open a terminal in the extracted folder and run
   **`./40WinksRecompiled`** (start it from that folder: it needs `assets/`).
3. On first launch, a window asks for your ROM. It is checked (a wrong dump is refused with the reason) and copied to
   the settings folder below.
4. Start the game from the launcher.

Settings, controller mappings, the ROM and saves are stored in `%LOCALAPPDATA%\40WinksRecompiled\` (Windows) or
`~/.config/40WinksRecompiled/` (Linux).

**Requirements:** Windows 10/11 (64-bit) or Linux x86-64 (tested on Ubuntu with X11) · GPU with Direct3D 12.0
(Shader Model 6) or Vulkan 1.2 (Vulkan on Linux) · CPU with AVX. No macOS build yet.
Linux also needs SDL2, GTK 3, FreeType, libcurl and the Vulkan loader; on Ubuntu / Debian:
`sudo apt install libsdl2-2.0-0 libgtk-3-0 libfreetype6 libcurl4 libvulkan1`.
If the game crashes on startup, update your graphics drivers first.

## 🐛 Known issues

- Only Worlds 1 and 2 have been fully tested.
- Screen fades stay at 25 frames per second.
- A few launcher options are greyed out for now (main volume, graphics API, mods).

## 📣 Reporting a bug

Open an [issue](https://github.com/DreaXer/40WinksRecomp/issues) with:

- the version (for example `0.4.0`) and your Windows version or Linux distribution;
- where it happened (world, level, area) and what you did just before;
- what happened (freeze, crash, black textures, wrong sound…), with a screenshot if possible;
- your **Framerate** and **Aspect Ratio** settings;
- for a crash, the `40winks-log.txt` file next to the executable (on Linux, in the folder you started it from).

## 🛠️ Building

Building needs your own ROM: the game's code is disassembled from it, then recompiled. The disassembly and
recompilation steps below run on Windows; Linux builds reuse their output (see *Building on Linux*).

**Requirements:** Visual Studio 2026 (version 18) with the C++ workload and clang-cl, Python 3 with
[splat](https://github.com/ethteck/splat) (`pip install splat64[mips]`), and MSYS2 with `mingw-w64-x86_64-clang` and
LLVM's `ld.lld` (for the ROM relink check).

```
git clone --recursive https://github.com/DreaXer/40WinksRecomp.git
```

1. Copy your ROM to the repository root as `baserom.eur.z64` (SHA-1 above).
2. Disassemble and check the relinked ROM matches (from `decomp/`):
   ```
   python -m splat split 40winks.yaml
   python -I tools/build_rom.py
   ```
   The second command must print `OK: ROM matches`. It writes `decomp/build/40winks.elf`.
3. Build `N64Recomp.exe` and `RSPRecomp.exe` from `lib/N64Recomp` (see its README) and copy them to the repository root.
4. Recompile the game and the audio microcode (from the repository root):
   ```
   ./N64Recomp.exe 40winks.toml
   ./RSPRecomp.exe aspMain.toml
   ```
5. Run `build.bat`. The executable is written to `build-vs/40WinksRecompiled.exe`.

### 🐧 Building on Linux

1. Install the build tools (Ubuntu / Debian):
   ```
   sudo apt install git build-essential cmake ninja-build clang lld llvm libsdl2-dev libgtk-3-dev libfreetype-dev libvulkan-dev libx11-dev libxrandr-dev libcurl4-openssl-dev
   ```
2. Clone with `--recursive`, then copy `RecompiledFuncs/` and `rsp/aspMain.cpp` from a Windows checkout where steps 1
   to 4 above were done.
3. Run `./build-linux.sh`. The executable is written to `build-linux/40WinksRecompiled`; run it from the repository
   root. `./build-linux.sh package` also writes the release archive to `dist/`.

## 📚 Libraries used and projects referenced

- [N64: Recompiled](https://github.com/N64Recomp/N64Recomp) and
  [N64ModernRuntime](https://github.com/N64Recomp/N64ModernRuntime) (fork:
  [DreaXer/N64ModernRuntime](https://github.com/DreaXer/N64ModernRuntime), branch `40winks`)
- [RT64](https://github.com/rt64/rt64) (fork: [DreaXer/rt64](https://github.com/DreaXer/rt64), branch `40winks`)
- [RecompFrontend](https://github.com/Killklli/RecompFrontend) (fork:
  [DreaXer/RecompFrontend](https://github.com/DreaXer/RecompFrontend), branch `40winks`)
- [Donkey Kong 64: Recompiled](https://github.com/Rainchus/Donkey-Kong-64-Recompiled), whose layout and launcher this
  project is based on, and [Zelda 64: Recompiled](https://github.com/Zelda64Recomp/Zelda64Recomp)
- [splat](https://github.com/ethteck/splat) and [spimdisasm](https://github.com/Decompollaborate/spimdisasm)
- [SDL2](https://www.libsdl.org/), [RmlUi](https://github.com/mikke89/RmlUi), [SlotMap](https://github.com/SergeyMakeev/SlotMap)

Launcher artwork: photos by DreaXer, inspired by the game.

<details>
<summary>🇫🇷 <b>En français</b></summary>

<br>

**40 Winks : Dreamcompiled** est un portage natif PC (Windows et Linux) du jeu N64 40 Winks, fait par recompilation
statique avec N64: Recompiled et rendu avec RT64 (haute fréquence d'images, écran large…).

- **Il faut ta propre copie du jeu** : version finale N64 de Piko Interactive (Europe), code `N4WZ`,
  SHA-1 `282b97c6a6eb79535e618fc2651b6156fff65ea4`. Ce dépôt et ses releases ne contiennent aucune donnée du jeu.
- **Version de test** : les mondes 1 et 2 ont été joués en entier sans bug ; la suite n'est pas encore testée. Garde une
  copie de tes sauvegardes (`%LOCALAPPDATA%\40WinksRecompiled\saves` ou `~/.config/40WinksRecompiled/saves`).
- **Installation** : extraire le zip (Windows) ou le tar.gz (Linux), lancer `40WinksRecompiled.exe` ou, sous Linux,
  `./40WinksRecompiled` depuis le dossier extrait, puis choisir la ROM quand c'est demandé. Linux demande aussi
  `sudo apt install libsdl2-2.0-0 libgtk-3-0 libfreetype6 libcurl4 libvulkan1` (Ubuntu / Debian).
- **Bugs** : ouvre une [issue](https://github.com/DreaXer/40WinksRecomp/issues) (version, système, endroit, ce que tu
  faisais, capture, réglages Framerate / Aspect Ratio, et `40winks-log.txt` pour un plantage).

</details>

---

<sub>40 Winks is the property of its respective owners. This project is not affiliated with them. Licensed under GPL-3.0, see [LICENSE](LICENSE).</sub>
