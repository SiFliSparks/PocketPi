# PocketPi

[English](README_EN.md) | [中文](README.md)

PocketPi is a small embedded games/demo project based on the SiFli platform. This repository contains source code, build scripts and resource folders needed to build and run on supported boards.

## Supported board

- Lichuang SF32LB52-ULP development board (reference): https://lckfb.com/project/detail/lckfb-hspi-sf32lb52-ulp?param=baseInfo

## Quick start

### Clone

```bash
git clone https://github.com/SiFliSparks/PocketPi.git
```

### Build and flash

Change into the `project/` directory and run the SCons build command (replace board name as needed):

```powershell
cd "d:\SIFLI\project\lchspi-extension\PocketPi\project"
scons --board=sf32lb52-lchspi-ulp -j32
```

After building, use the provided download script to flash the device (example for UART download):

```powershell
build_sf32lb52-lchspi-ulp_hcpu\uart_download.bat
# follow prompts and select the serial port
```

## Running games

After flashing and booting the device, the firmware scans the `disk/` directory and lists available files. Notes:

- You must prepare ROM or binary files locally and put them into the repository `disk/` folder for testing. Do NOT commit copyrighted ROM files to the repository.
- On boot the UI displays a file list from `disk/`. Select an item in the list to run it.

Example flow:

1. Copy a test ROM into the repository `disk/` directory.
2. Boot the device and wait for the UI to scan the `disk/` directory.
3. Select a file from the list to launch it.

## Screenshots

Game list view:
![game list](assets/game_list.jpg)
Running game:
![game running](assets/game_running.jpg)

## Directory structure

Key files and folders:

- `disk/`: runtime resource directory scanned by the firmware. Put local test ROMs/binaries here (do not commit copyrighted ROMs).
- `project/`: build scripts, board configs and build outputs (generated `build_*` folders appear here after building).
- `src/`: project source code (C files and third-party submodules).
- `assets/`: example images and screenshots used in the README.
- `.gitignore`: ignore rules (build outputs, common ROM extensions, etc.).
- `README_EN.md` / `README.md`: project documentation.

Typical flow: edit `disk/` → run `scons` in `project/` → flash using the download script → select and run on device.

## Hardware buttons and IO

Physical game buttons are exposed through GPIO pins. Implementation details live in `src/video_audio.c` (and `src/main.c`). Highlights:

- GPIO pins used for keys:
  - `int key_pin_def[] = {30,24,25,20,10,11,27,28,29};` (9 keys, configured as input with pull-up)
- Scanning and debouncing:
  - A software timer at ~200 Hz calls `key_scan()` to poll keys.
  - Each key uses an 8-bit shift buffer. `0xFF` indicates stable pressed, `0x00` indicates released (debounce logic).
- Key index → game/system mapping (see `ConvertGamepadInput()`):
 - Key index → GPIO, function and Select-combos (summary):

| Index | GPIO Pin | Key name | Description | Select+ combo (when Select held) |
|------:|:--------:|:--------|:-----------|:---------------------------------|
| 0 | 30 | Select | Main modifier key used for system shortcuts | — |
| 1 | 24 | Unused | Currently no assignment | — |
| 2 | 25 | Start | Game Start button | Select + Start → trigger quit (`trigger_quit()`) |
| 3 | 20 | B | Game B button | Select + B → load state (`state_load()`) |
| 4 | 10 | A | Game A button | Select + A → save state (`state_save()`) |
| 5 | 11 | Up | Up / menu up | Select + Up → decrease audio shift (`audio_shift_bits--`) |
| 6 | 27 | Right | Right / menu right | Select + Right → next save slot (`selectedSlot++`, calls `state_setslot()`) |
| 7 | 28 | Left | Left / menu left | Select + Left → previous save slot (`selectedSlot--`, calls `state_setslot()`) |
| 8 | 29 | Down | Down / menu down | Select + Down → increase audio shift (`audio_shift_bits++`) |

Note: The Select+combo actions are implemented in `ConvertGamepadInput()` and are detected as "Select held + target key press event".
- Debug / API:
  - `get_key_state(index)` — read current state (0/1)
  - `get_key_press_event(index)` / `get_key_release_event(index)` — read and clear one-shot events
  - MSH command `key_set 0xXXXX` — force `key_state` value for debugging
- Wiring suggestion: one side of each button to the specified GPIO pin, the other side to GND. Pins are configured with internal pull-up; press pulls pin low.

To change pins or mapping, edit `key_pin_def[]` or `ConvertGamepadInput()` in `src/video_audio.c` and rebuild.

## Troubleshooting (FAQ)

Q: No files in the game list?
A: Make sure you have copied your test ROM or binary files into the repository `disk/` directory, and that their file extensions are supported (for example, `.nes`). If the directory is empty or contains no supported files, the list will not display any items.

If your problem is not listed, attach serial logs and steps to reproduce and open an issue.

## Issues

If you find a bug or have suggestions, please open an issue on GitHub: https://github.com/SiFliSparks/PocketPi/issues
