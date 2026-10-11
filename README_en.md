# th06UtilityTool

> [!NOTE]
> **Japanese Documentation:** [日本語版はこちら (README.md)](README.md)

A performance enhancement and feature extension patch for *Touhou Koumakyou ~ the Embodiment of Scarlet Devil* ver 1.02h (SHA-256: `9f76483c46256804792399296619c1274363c31cd8f1775fafb55106fb852245`).

---

## Installation & How to Play

1. Download the latest `th06UtilityTool.zip` from [Releases](../../releases) and extract it.
2. Copy the extracted files (`launcher.exe`, `kouma_patch.delta`, `th06_spell_practice.dll`, `custom_new.ini`) into your official **Touhou Koumakyou ver 1.02h** game directory (where `東方紅魔郷.exe` is located).
3. Run `launcher.exe`. The patch will be applied automatically and launch the enhanced game.

---

## Key Features

- **Display & Graphics**
  - Borderless windowed fullscreen mode (seamless multi-monitor experience without minimizing)
  - Video capture & streaming support (OBS Studio, Discord, etc.)
  - Frame rate stabilization and increased display refresh frequency
  - Instant toggle between windowed and fullscreen modes via Alt + Enter
  - Custom window resolution settings (`window_width`, `window_height`)
  - Automatic saving and restoring of window position (`window_pos_x`, `window_pos_y`)
  - Vertical synchronization (VSync) toggle
  - Circular boss HP gauge display (`boss_hp_circle`)
  - Enemy marker (triangle indicator at screen bottom, `enemy_marker`)
  - In-game difficulty display in the upper right corner (`show_difficulty`)
  - Real-time frame counter display (`show_frame_counter`)
  - TATE / Vertical display mode (90-degree clockwise or counter-clockwise rotation)
  - Fixed display freeze issue after resuming from system sleep/standby

- **Controls & Input**
  - Significantly reduced input lag
  - Disabled IME (Half-width / Full-width) key switching when game window is focused (prevents accidental key lock)
  - Prevention of unintended key inputs immediately upon game launch
  - Quick game exit by pressing X on the title screen when cursor is on "Quit"
  - Fully customizable keybindings via `custom_new.ini` using DirectInput scan codes
  - Dialogue skip button mapping (`skip`, default: LCtrl)
  - Arcade stick friendly button layout (shift focus button to the far right via `move_shift`)

- **Gameplay, Practice & Networking**
  - Built-in Spell Practice mode via companion DLL (Hold Shift and select stage in Practice menu)
  - Online multiplayer support (P2P Host/Guest matching for network co-op / versus)
  - Compatibility with thprac replays
  - Fast-forward replay playback (up to 5x speed)
  - Quick retry in pause menu via R key or simultaneous press of "Left + Shoot + Focus"
  - Quick quit to title in pause menu via Q key
  - Player and bullet hitbox visualization (`show_collision`, `hitdisp`)
  - Real-time input display overlay (`show_input`)

- **Audio & System**
  - **In-Game Quick Settings Overlay Menu (F1 key)**
    - Change graphics, audio, system, and BGM track settings on the fly without interrupting gameplay
  - Independent volume adjustment for BGM (WAV playback) and Sound Effects (0–100%)
  - Independent volume adjustment for MIDI BGM (0–100%)
  - Per-track BGM playback mode selection (MIDI or WAV, customizable for all 17 tracks)
  - Built-in SoundFont synthesizer (FluidSynth) support (high-quality MIDI without external MIDI hardware)
  - User-configurable SoundFont directory with in-game GUI file & folder picker dialogs
  - BGM pause and seamless resume at the exact playback position when unpausing
  - Automatic generational save data backup for `score.dat` (configurable retention count)

---

## Feature Details

### Spell Practice
With `th06_spell_practice.dll` placed in the game directory, **hold Shift while selecting a stage in the Practice menu** to bring up the Spell Practice menu. You can select and practice any individual spell card.

### Online Play (Experimental)
Supports peer-to-peer multiplayer over local network (different network / WAN play is currently undergoing verification).  
During standard character selection, **hold Shift while confirming your shot type** to display "Host" and "Guest" options and configure matching.

---

## Configuration File (custom_new.ini)

The bundled `custom_new.ini` stores settings specific to this patch in plain text and can be edited with any text editor. In general, `0` disables an option and `1` enables it.

All settings can also be modified in real-time via the **in-game Quick Settings menu by pressing F1**. Settings are categorized into Display, System, Audio, and BGM Type.

### [display]

| Setting | Description |
|---|---|
| `show_input` | Displays real-time controller/keyboard inputs in the lower right corner (4 directions, Shoot, Bomb, Focus). |
| `move_shift` | Moves the Focus button display to the far right (recommended for arcade stick layouts). |
| `window_width` | Specifies custom window width in pixels. |
| `window_height` | Specifies custom window height in pixels. |
| `window_pos_x` | Specifies initial window X coordinate (automatically updated on exit). |
| `window_pos_y` | Specifies initial window Y coordinate (automatically updated on exit). |
| `show_collision` | Visualizes player hitbox. *Note: Enabling this disables saving scores and replays.* |
| `hitdisp` | Visualizes bullet hitboxes with red outlines. *Note: Disables score and replay saving.* |
| `vsync` | Enables VSync. Reduces tearing at the cost of potential input latency. |
| `boss_hp_circle` | Displays a circular ring indicator showing remaining boss HP. |
| `enemy_marker` | Displays an enemy position marker triangle at the screen bottom. *Note: Disables score and replay saving.* |
| `show_difficulty` | Displays the current difficulty rank (Normal, Hard, etc.) in the upper right corner. |
| `show_frame_counter` | Displays elapsed frame count in real-time in the upper right corner. |
| `vertical_mode` | Enables TATE/vertical mode (`1`: 90 degrees clockwise, `2`: 90 degrees counter-clockwise). |

### [system]

| Setting | Description |
|---|---|
| `backup` | Automatic backup for save data (`score.dat`) (`0`: disabled, `1`: enabled). |
| `backup_keep_count` | Maximum number of backup generations to retain (default: `30`). |

### [audio]

| Setting | Description |
|---|---|
| `bgm_volume` | Volume for BGM during WAV playback (0–100). |
| `se_volume` | Volume for Sound Effects (0–100). |
| `internal_synth` | Uses internal SoundFont synth (FluidSynth) (`1`: internal FluidSynth, `0`: external MIDI synth / WinMM). |
| `bgm_volume_midi` | Volume for BGM during MIDI playback (0–100). |
| `soundfont_dir` | Directory path to search for SoundFont (`.sf2`) files (default: `soundfonts`). Can also be selected via GUI in F1 menu. |
| `sf2` | Filename of the SoundFont to use (filename only, not full path). Can be cycled with left/right or picked via GUI file dialog in F1 menu. If empty or file not found, the embedded `GeneralUser-GS.sf2` is used automatically. |

### [bgm]

| Setting | Description |
|---|---|
| `track01` – `track17` | Playback mode for each specific song (`0`: MIDI, `1`: WAV). Can also be adjusted individually in the F1 menu. |

### [input]

Key mappings can be customized using DirectInput scan codes (default keys used if unspecified).

| Setting | Default Key | Scan Code |
|---|---|---|
| `shoot` | Z | 44 |
| `bomb` | X | 45 |
| `focus` | LShift | 42 |
| `menu` | Escape | 1 |
| `up` | Up / W | 200 / 17 |
| `down` | Down / S | 208 / 31 |
| `left` | Left / A | 203 / 30 |
| `right` | Right / D | 205 / 32 |
| `skip` | LCtrl | 29 |

<details>
<summary>DirectInput Scan Code Reference Table</summary>

| Code | Key | Code | Key | Code | Key | Code | Key |
|---|---|---|---|---|---|---|---|
| 1 | Escape | 2 | 1 | 3 | 2 | 4 | 3 |
| 5 | 4 | 6 | 5 | 7 | 6 | 8 | 7 |
| 9 | 8 | 10 | 9 | 11 | 0 | 12 | Minus |
| 13 | Equals | 14 | Back | 15 | Tab | 16 | Q |
| 17 | W | 18 | E | 19 | R | 20 | T |
| 21 | Y | 22 | U | 23 | I | 24 | O |
| 25 | P | 26 | LBrack | 27 | RBrack | 28 | Return |
| 29 | LCtrl | 30 | A | 31 | S | 32 | D |
| 33 | F | 34 | G | 35 | H | 36 | J |
| 37 | K | 38 | L | 39 | Semi | 40 | Apost |
| 41 | Grave | 42 | LShift | 43 | BackSl | 44 | Z |
| 45 | X | 46 | C | 47 | V | 48 | B |
| 49 | N | 50 | M | 51 | Comma | 52 | Period |
| 53 | Slash | 54 | RShift | 56 | LAlt | 57 | Space |
| 58 | Caps | 59 | F1 | 60 | F2 | 61 | F3 |
| 62 | F4 | 63 | F5 | 64 | F6 | 65 | F7 |
| 66 | F8 | 67 | F9 | 68 | F10 | 87 | F11 |
| 88 | F12 | 157 | RCtrl | 184 | RAlt | 71 | Num7 |
| 72 | Num8 | 73 | Num9 | 75 | Num4 | 76 | Num5 |
| 77 | Num6 | 79 | Num1 | 80 | Num2 | 81 | Num3 |
| 82 | Num0 | 200 | Up | 208 | Down | 203 | Left |
| 205 | Right | | | | | | |

</details>

---

## Important Notes & Disclaimer

> [!WARNING]
> - This software is an unofficial fan-made project and is not affiliated with ZUN or Team Shanghai Alice. Unexpected issues may cause save data corruption.
> - The developers assume no liability for any damages or data loss resulting from the use of this patch.
> - This software does not contain any original game executables, images, sounds, or stage data (a legally owned copy of Touhou 06 ver 1.02h is required to play and build).
> - Regular backup of your save data (`score.dat`) is strongly recommended.

## Building from Source (Developers)

This project can be built either on Linux (Ubuntu / WSL2) or natively on Windows.

---

### A. Linux / WSL2 Build (GNU Make)

#### Prerequisites
- Linux environment (Ubuntu / Debian, WSL2 supported)
- `gcc-mingw-w64-i686`, `g++-mingw-w64-i686`, `binutils-mingw-w64-i686`
- `cmake` (3.13 or newer)
- `python3`
- `lld` (recommended)
- `ccache` (recommended)
- `zip`

```bash
# Example dependency installation on Ubuntu / Debian:
sudo apt update
sudo apt install -y gcc-mingw-w64-i686 g++-mingw-w64-i686 binutils-mingw-w64-i686 cmake python3 lld ccache zip
```

#### Build Steps
```bash
# Clone with submodules
git clone --recurse-submodules https://github.com/Madotsukanai/th06util
cd th06util

# Place your original Touhou Koumakyou ver 1.02h executable (th06.exe) as resources/game.exe
# (Required for required game data extraction and linking)
cp /path/to/th06.exe resources/game.exe

# Build all targets (asset extraction and parallel build)
make

# Create release package (build/th06UtilityTool.zip)
make package
```

---

### B. Windows Native Build (NMAKE)

You can build natively on Windows using Microsoft NMAKE and the provided `Nmakefile`.

#### Prerequisites
- **Python 3** (Windows installer, ensure added to PATH)
- **MinGW-w64 (32-bit i686)** (from MSYS2 `mingw-w64-i686-gcc` or [w64devkit](https://github.com/skeeto/w64devkit))
- **CMake** (for building FluidSynth static library)
- **NMAKE** (included with Visual Studio Developer Command Prompt or Windows SDK)
- **PowerShell** (for release ZIP packaging, Windows default)

#### Build Steps
Open a Command Prompt (e.g. Developer Command Prompt for VS) and run:

```cmd
:: Clone with submodules
git clone --recurse-submodules https://github.com/Madotsukanai/th06util
cd th06util

:: Place your original Touhou Koumakyou ver 1.02h executable (th06.exe) as resources\game.exe
:: (Required for required game data extraction and linking)
copy C:\path\to\th06.exe resources\game.exe

:: Build all targets (th06util.exe, launcher.exe, patch, dll)
nmake /f Nmakefile

:: Create release ZIP package (build\th06UtilityTool.zip)
nmake /f Nmakefile package
```

---

## Licenses

This software (th06util) is dedicated to the public domain under the [Creative Commons CC0 1.0 Universal](LICENSE) (Public Domain Dedication).

For complete license terms of this project and third-party components, please refer to [LICENSE](LICENSE).

- **th06util**: CC0 1.0 Universal (Public Domain Dedication)
- **th06 (GensokyoClub)**: CC0 1.0 Universal (Public Domain)
- **FluidSynth**: GNU Lesser General Public License version 2.1 (LGPL 2.1)
- **xdelta3**: Apache License version 2.0
- **d3d8to9**: BSD 2-Clause License
- **GeneralUser GS SoundFont**: S. Christian Collins (Free License for Software Projects)
