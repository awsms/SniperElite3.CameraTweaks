# Sniper Elite 3 Camera Tweaks 2.0

A DirectX 11 ASI plugin that adds a free camera and an in-game FOV slider to
Sniper Elite 3. Based on [ermaccer's FreeCamera](https://github.com/ermaccer/SniperElite3.FreeCamera).

## Build

Requirements:

- Visual Studio 2022 with **Desktop development with C++** and a Windows SDK.
- CMake 3.20 or newer.

From a Developer Command Prompt in the repository directory:

```bat
cmake -S . -B build -A Win32
cmake --build build --config Release
```

The game is **32-bit**: use `Win32`, not `x64`. The output files are:

```text
build/Release/SniperElite3.CameraTweaks.asi
build/Release/SniperElite3.CameraTweaks.ini
```

Dear ImGui and MinHook are included in `third_party`; no dependency downloads
are required. Their licenses are included alongside their sources.

## Install

Download the Win32 package from [Releases](https://github.com/awsms/SniperElite3.CameraTweaks/releases).

1. Close the game.
2. Copy `SniperElite3.CameraTweaks.asi` and `SniperElite3.CameraTweaks.ini` into the
   game's **`bin` directory**, next to `SniperElite3.exe`.
3. Install the **Win32/x86 `dinput8.dll`** from
   [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases)
   in the same directory if an ASI loader is not already installed. The loader
   is not included with this plugin. Preserve an existing loader and merge an
   existing FreeCamera configuration instead of overwriting custom settings.
4. Launch the game using DirectX 11.

Example layout:

```text
Sniper Elite 3/
  bin/
    SniperElite3.exe
    dinput8.dll
    SniperElite3.CameraTweaks.asi
    SniperElite3.CameraTweaks.ini
```

**Upgrading from FreeCamera:** remove the old `SniperElite3.FreeCamera.asi`
from the game directory so both plugins do not load together. Rename your old
`SniperElite3.FreeCamera.ini` to `SniperElite3.CameraTweaks.ini` to keep your settings.

This plugin does not replace `d3d11.dll`. Keep the DLLs and other files
required by your existing mods in place.

**Linux/Proton only:** the native ASI loader may need a DLL override. Add
`dinput8=n,b` to `WINEDLLOVERRIDES`; for example, if no other overrides are set:

```text
WINEDLLOVERRIDES="dinput8=n,b" %command%
```

Preserve any existing DLL overrides required by your other mods.

## Use

- **F6:** open or close the FOV menu. Pause the game first, then use the mouse
  or Tab/arrow keys to adjust the slider.
- **F5:** enable or disable the free camera.
- **Numpad 8/2/4/6/7/1:** move the free camera.
- **Numpad 5:** slow down; **Numpad 9:** speed up.

The FOV slider ranges from **0.50x to 2.00x**. `1.00x` restores the original
value. This is a multiplier, not a field-of-view angle in degrees.
**Reload the checkpoint after changing, disabling, or resetting FOV.**

Moving the slider applies the value in memory. Saving the setting stores a
preference in the INI; at the next launch, open the menu and apply it explicitly.
FOV control works independently of free-camera mode. Close the menu with its
hotkey, Escape, or the close button.

Keys are configurable using decimal Windows virtual-key codes in the INI.
The defaults use F6 for the FOV menu, F5 instead of FreeCamera's original F1
binding, and Numpad 9 instead of its original Numpad 0 acceleration binding.
Adjust these shortcuts if they conflict with your existing mods.

If the game's expected FOV constant is not found, or another mod changes it,
the menu disables FOV writes and displays an explanation. Do not use another
FOV trainer at the same time. This patch still requires a checkpoint reload;
it does not update the active camera projection immediately.

To uninstall, close the game and remove this plugin's `.asi` and `.ini` files.
Keep the ASI loader if other installed plugins depend on it.
