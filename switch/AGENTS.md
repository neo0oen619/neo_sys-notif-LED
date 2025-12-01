This folder mirrors the root of a Nintendo Switch SD card for this project.

- `atmosphere/contents/0100000000000895/exefs.nsp`
  - Sysmodule binary for `sys-notif-LED` (title id `0100000000000895`).
  - Built from `sysmodule/` with devkitPro/libnx.
  - When updating sysmodule code, rebuild and copy the new `sys-notif-LED.nsp` here as `exefs.nsp`.

- `config/sys-notif-LED/`
  - Configuration directory used at runtime (`settings.cfg`, `type`, etc.).
  - Created/updated on console; this repo usually only needs defaults or examples.

- `switch/sys-notif-LED-config.nro`
  - Homebrew config app for Smart mode.
  - Built from `config_app/` with devkitPro/libnx.
  - When updating the config app, rebuild and overwrite this NRO.

- `switch/screenoff.nro`
  - Screen-off helper app.
  - Built from `screen_off_app/` with devkitPro/libnx.
  - Turns the LCD backlight off while keeping the system awake, and coordinates sleep based on Smart full-timeout or a 10-minute fallback.

- `switch/.overlays/sys-notif-LED.ovl`
  - Tesla overlay for quick LED mode selection.

Guidance for agents (very important):

- Treat this `switch/` tree as a deployment layout, not as source code.
- Never hand-edit binaries (`*.nsp`, `*.nro`, `*.ovl`). Instead:
  - Make changes under `sysmodule/`, `config_app/`, `screen_off_app/`, or `overlay/`.
  - Then rebuild using the user’s devkitPro install at `C:\devkitPro`:
    - Sysmodule:
      - `C:\devkitPro\msys2\usr\bin\bash -lc "cd /c/aiwork/sys-notif-LED-main/sysmodule && make clean all"`
    - Config app:
      - `C:\devkitPro\msys2\usr\bin\bash -lc "cd /c/aiwork/sys-notif-LED-main/config_app && make clean all"`
    - Screen-off helper:
      - `C:\devkitPro\msys2\usr\bin\bash -lc "cd /c/aiwork/sys-notif-LED-main/screen_off_app && make clean all"`
  - After a successful build, always refresh the deployment files:
    - Copy `sysmodule/sys-notif-LED.nsp` -> `switch/atmosphere/contents/0100000000000895/exefs.nsp`
    - Copy `config_app/sys-notif-LED-config.nro` -> `switch/switch/sys-notif-LED-config.nro`
    - Copy `screen_off_app/screenoff.nro` -> `switch/switch/screenoff.nro`
- Keep this directory structure compatible with Atmosphère so the user can copy this `switch/` folder directly to the SD card root without further rearranging.

