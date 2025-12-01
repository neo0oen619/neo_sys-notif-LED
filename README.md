# sys-notif-LED

Sysmodule to enable and control the notification LED on boot or when new controllers are added.

You can control it either from the included overlay or from the config homebrew app.

## Modes (overlay)

The overlay lets you select the base mode:

- Solid
- Dim
- Fade
- Off
- Dim when charging
- Dim when battery below 15%, blink when below 5%
- Smart charge/battery mode

## Smart charge/battery mode (auto mode)

When *Smart* is selected, the sysmodule uses battery/charging state and your config to decide what the LED does:

- **Charging < 100%**: uses a configurable pattern and interval (default: continuous fade).
- **Charging at 100%**: shows a configurable notification pattern on an interval until a configurable *full timeout* (minutes) is reached, then turns off.
- **On battery, above low threshold**: LED is normally off, but can:
  - Show a short idle pattern every `discharge_interval_seconds` seconds for `discharge_duration_seconds` seconds (for fun / presence).
  - Show a configurable notification pattern whenever the battery drops by a configurable step (e.g. every 10%).
- **On battery, below low threshold**: uses a configurable low-battery pattern and interval (default: continuous blink below 15%).

## Config app (sys-notif-LED-config)

The included NRO sys-notif-LED-config lets you configure Smart mode from the homebrew menu and also switch the active mode to **Smart** without using the overlay. All sliders are used by the sysmodule in Smart mode, and some now have live timers:

- **Full / Charging**
  - Full-charge timeout (minutes).
  - Full-charge notification interval (seconds, 0 = continuous).
  - Full-charge pattern: Off / Solid / Dim / Fade / Blink.
  - Charging <100% interval (seconds, 0 = continuous).
  - Charging <100% pattern: Off / Solid / Dim / Fade / Blink.
- **Not charging**
  - Idle interval (seconds, 0 = disabled): how often to show an idle pattern while unplugged and not low.
  - Idle duration (seconds, 0 = default): how long the idle pattern stays on each time.
  - Idle pattern: Off / Solid / Dim / Fade / Blink.
  - Drop event step in % (e.g. 10% or 5%) and a pattern + duration to show on each step.
- **Low battery**
  - Low-battery threshold (%).
  - Low-battery interval (seconds, 0 = continuous).
  - Low-battery pattern: Off / Solid / Dim / Fade / Blink.
- **Logging**
  - Logging enable/disable flag (log_enabled). On the main screen, **X** toggles logging on/off for the config app and saves to settings.cfg.
- **Mode**
  - On the main screen, press **Y** to set the current mode to **Smart** (writes type=smart and a reset flag for the sysmodule).

The LED continues to run its patterns while the system is awake; only a full system sleep stops the sysmodule from updating state.

### Smart full-timeout sleep and timers

When Smart is active, the config app shows live countdowns:

- On **Full / Charging**, the `Full timeout` line displays the remaining time until the Smart full-charge timeout expires.
- On **Not charging**, the `Idle interval` line displays the remaining seconds until the next idle pattern burst when the idle timer is active.

In **System / Logging** you also have:

- **Sleep LED**: `Keep pattern` / `Off at timeout`
  - Controls whether the LED is turned off or kept as-is when the Smart full-timeout expires.
- **Full timeout**: `Sleep` / `No sleep`
  - When combined with the `screenoff` helper (see below), the console will be sent to real sleep when the Smart full-timeout reaches zero.

After a Smart full-timeout–triggered sleep via `screenoff`, waking the console will restart the full-timeout cycle and allow the LED to show its notifications again while charging at 100%.

## Screen-off helper (screenoff.nro)

The `switch/screenoff.nro` app provides a “screen off but stay awake” mode:

- Turns the LCD backlight off while keeping Horizon and all sysmodules (including sys-notif-LED) running.
- Disables auto-sleep while it runs so Smart timers and LED patterns continue.
- Works with two sleep strategies:
  - **Smart sleep**: if Smart full-timeout + `Full timeout: Sleep` is configured, `screenoff` watches the sysmodule’s status file and requests sleep exactly when the Smart full-timeout reaches zero. On wake, it restores the backlight/auto-sleep and drops a flag so the Smart full-timeout cycle restarts.
  - **Fallback sleep**: if Smart sleep is not configured or the status file is unavailable, `screenoff` puts the console to sleep after ~10 minutes with the screen off.
- Shows a purple credit line anchored at the bottom-right: `made with <3 by neo0oen619`.

Place this app at `switch/switch/screenoff.nro` so it appears in the homebrew menu. You can optionally wrap it in an NSP forwarder if you want a HOME menu icon.

