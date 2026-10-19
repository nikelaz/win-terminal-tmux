# wt-tmux-tray

A small Windows system tray program that adds tmux-style `Ctrl+B` prefix shortcuts to Windows Terminal.

While Windows Terminal is the focused window, press `Ctrl+B`, release it, then press one of the keys below to create tabs, split panes, switch tabs, move focus, or close panes. The program translates each tmux-style key into the matching Windows Terminal shortcut.

## Requirements

- Windows 10/11
- [Windows Terminal](https://aka.ms/terminal) with its **default keybindings**
- Visual Studio (2019 or newer) with the **Desktop development with C++** workload, for building

## Building

Run from the project folder:

```bat
build.bat
```

The script compiles `main.cpp` and writes the executable to `dist\wt-tmux-tray.exe`. The `dist` folder is created if needed.

The script locates MSVC through `vcvars64.bat` at this path:

```
C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat
```

If your Visual Studio is installed elsewhere or is a different version, edit the `VCVARS` line at the top of `build.bat`.

## Running

Double-click `dist\wt-tmux-tray.exe`. The program starts in the background with no window, and a tray icon appears in the notification area.

- Only one instance runs at a time. Starting it again does nothing.
- To stop it, right-click the tray icon and choose **Exit**.
- To pause the shortcuts without exiting, right-click the tray icon and uncheck **Enable tmux keys (Ctrl+B)**. Check it again to re-enable.

The program does not start automatically at sign-in. To run it at startup, place a shortcut to `wt-tmux-tray.exe` in the Startup folder (`Win+R`, then `shell:startup`).

## Keyboard shortcuts

Press `Ctrl+B`, release it, then press the second key within 2 seconds.

| Keys            | Action                                    | Windows Terminal shortcut sent |
|-----------------|-------------------------------------------|--------------------------------|
| `Ctrl+B`, `C`   | Open a new tab                            | `Ctrl+Shift+T`                 |
| `Ctrl+B`, `%`   | Split the pane left | right (50% / 50%)   | `Alt+Shift+Plus`               |
| `Ctrl+B`, `1`–`8` | Switch to tab 1–8 (1-based index)       | `Ctrl+Alt+1` … `Ctrl+Alt+8`    |
| `Ctrl+B`, `9`   | Switch to the **last** tab                | `Ctrl+Alt+9`                   |
| `Ctrl+B`, `O`   | Toggle focus between split panes          | `Ctrl+Alt+Left` (previous pane) |
| `Ctrl+B`, `X`   | Close the focused pane, or the tab if it is the last pane | `Ctrl+Shift+W`  |

Notes:

- `%` is typed as `Shift+5` on a US keyboard layout.
- `9` follows Windows Terminal's default binding, which selects the last tab rather than the ninth tab.
- Pressing `Ctrl+B` followed by any other key cancels the prefix. That key is then passed through normally.
- Holding `Ctrl+B` does not re-trigger the prefix.
- The shortcuts only apply while Windows Terminal is the foreground window. Other applications receive `Ctrl+B` as usual.

## Usage tips

- If you remap any of the Windows Terminal shortcuts listed above, the matching `Ctrl+B` action will stop working or do something different. Restore the defaults, or edit `main.cpp` to send your custom keys.
- `Ctrl+B` followed by `O` relies on Windows Terminal's "previous pane" focus history. It toggles between the two most recently focused panes.
- The program does not send `Ctrl+B` to the shell. Shell and editor shortcuts that use `Ctrl+B` will not receive it while the program is running.

## Files

| Path                        | Purpose                              |
|-----------------------------|--------------------------------------|
| `main.cpp`                  | Program source                       |
| `build.bat`                 | Build script                         |
| `dist\wt-tmux-tray.exe`     | Built executable (created by build)  |
| `README.md`                 | This file                            |
