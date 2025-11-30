# sys-notif-LED

Sysmodule to enable the notification LED on boot or when new controllers are added.

The mode can be changed in the included overlay:

- Solid
- Dim
- Fade
- Off
- Dim when charging
- Dim when battery bellow 15%, blink when bellow 5%
- Smart charge/battery mode

## Smart charge/battery mode

This mode combines several behaviors into one automatic profile:

- While charging and **< 100%**: LED uses a **fade** pattern.
- While charging and **= 100%** for the first **~30 minutes**: LED alternates between **solid** and **blinking** to show that the battery is full.
- While charging and **= 100%** for **more than ~30 minutes**: LED turns **off** to reduce power usage and avoid keeping the LED on forever.
- On battery and **< 15%**: LED **blinks** continuously (low‑battery warning).
- On battery and **≥ 15%**: LED is **off** by default.
- On each new 10% drop while on battery (90->80->70->... and above 15%): LED goes to **fade** for about **10 seconds**, then returns to **off**.
- Unplugging the charger or dropping below 100% resets the full‑charge timer and smart mode returns to the normal charging/battery behavior above.

The smart mode is selectable in the overlay as **"Smart charge/battery mode"**.
