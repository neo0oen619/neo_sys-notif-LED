
# neo_sys‑notif‑LED 🎮✨  
> sys‑notif‑LED – because your Switch’s only light show shouldn’t be the joy‑con pairing animation.

## 🧠 Why Does This Exist?
The Nintendo Switch has a lonely notification LED inside the right Joy‑Con.  Nintendo uses it… never.  This project fixes that.  **neo_sys‑notif‑LED** is a sysmodule that hijacks the LED on boot and when controllers connect.  It then exposes controls via a Tesla overlay and a homebrew config app so you can turn it into a tiny status indicator:

- Glow when you plug in the charger.  
- Blink when your battery dips below “oh‑no” levels.  
- Pulse gently while you download yet another game you’ll never finish.  
- Or just stay off if flashing lights make you nervous.

It started as a fork of [Xc987’s sys‑notif‑LED](https://github.com/Xc987/sys-notif-LED).  I polished the code, added a Smart (auto) mode, a screen‑off helper app, and made sure nothing spontaneously combusts.  Think of it as a quality‑of‑life mod for your Switch’s notification LED.

## ⚙️ What Does It Actually Do?
- **Sysmodule:** runs in the background and updates the Joy‑Con notification LED based on charge status and battery level.
- **Overlay:** a Tesla overlay (`sys‑notif‑LED.ovl`) that lets you pick modes (Solid, Dim, Fade, Off, Dim on charge, Dim on low battery, or Smart) without leaving your game.
- **Smart Mode:** a brainy auto mode that reacts to charging/battery state:
  - Charging < 100% – continuous fade (because charging is boring).  
  - Charging at 100% – runs a configurable notification pattern until a full‑timeout expires, then goes off.  
  - On battery, above threshold – idle pattern every so often, or only when the percentage drops by a step you set.  
  - On battery, below low threshold – blinks a low‑battery pattern so you stop pushing your luck.  
- **Config App:** `sys‑notif‑LED‑config.nro` lives in the **Homebrew Menu** and lets you tune Smart mode: choose patterns (Solid/Dim/Fade/Blink), intervals, thresholds, idle burst duration, logging and more.
- **Screen‑Off Helper:** `screenoff.nro` turns off the LCD backlight while keeping Horizon and all sysmodules running.  Smart timers continue counting down.  When used with Smart + Full‑timeout: Sleep, it will request a proper sleep exactly when the timeout hits.  Otherwise it falls back to a normal sleep after ~10 minutes with the screen off.
- **Logging:** optional logging to `settings.cfg` to see exactly when patterns changed.  Great for debugging, or just watching your LED’s life story.

## 🚀 What’s New in 1.0.6w (“screenoff”)
*What we actually fixed (besides our sanity):*

- Added the **screenoff helper** app – press it to turn off your screen but keep the console awake.  No more killing the backlight using `hos`.  It coordinates with Smart full‑timeouts for a seamless “sleep when ready” experience.
- Added live timers in the config app to show remaining time until Smart full‑charge timeout and next idle burst.  Because waiting in the dark is boring.
- Added **Sleep LED** option: choose whether the LED turns off or stays as‑is when the Smart full‑timeout expires.
- Added **Full timeout** behaviour: Sleep or No sleep.  Combine with screenoff helper to control whether the console really sleeps when the full timeout hits.
- Added logging toggle in the config app.  Now you can yell “why are you blinking?” and check the log later.
- Many little bug fixes, performance tweaks, and battery‑friendly improvements.  My original code looked like spaghetti; this one is more al dente.

## 📦 How to Install (Assuming Atmosphère)
This repository mirrors the root of your Switch’s SD card.  To install:

1. **Download the latest release.** Grab the contents of this repo or the compiled package from the Releases page.
2. **Copy the files:**
   - `atmosphere/contents/0100000000000895/exefs.nsp` → the sysmodule binary.  Replace `exefs.nsp` if you rebuild from source.  Title ID `0100000000000895` is reserved for sys‑notif‑LED.
   - `config/sys‑notif‑LED/` → config directory.  It will be created/updated on first run.  You generally don’t need to copy anything here unless you want to start with our defaults.
   - `switch/sys‑notif‑LED‑config.nro` → the homebrew config app (Smart mode GUI).  Shows up in the Homebrew Menu.
   - `switch/.overlays/sys‑notif‑LED.ovl` → the Tesla overlay module for quick mode switching.
   - `switch/screenoff.nro` → the screen‑off helper.  Also appears in the Homebrew Menu.  Optional but highly recommended.
3. **Reboot your Switch.** If you did everything right, the sysmodule will start at boot.  Open the overlay (via Tesla) or the config app to confirm.

**Updating?** Replace the above files with the new versions.  Always reboot after updating a sysmodule.  It’s safer than hot‑patching your nervous system.

## 🕹️ How to Use It
- Hold **L + Down + DPad + Left Stick Click** (or whatever your Tesla hotkey is) to open the overlay and cycle between modes:  
  Solid, Dim, Fade, Off, Dim on charge, Dim on low battery, or **Smart**.  The LED should update instantly.
- Launch **sys‑notif‑LED‑config** from the Homebrew Menu to tweak Smart mode:
  - Set full‑charge timeout minutes and notification intervals.
  - Choose patterns (Off/Solid/Dim/Fade/Blink) for charging at 100% and idle bursts.
  - Set idle intervals/durations when not charging.  Set drop event steps (e.g. blink every 10% drop).
  - Configure low‑battery threshold, interval and pattern (e.g. blink continuously under 15%).
  - Toggle logging on/off.  Press **X** on the main screen.
  - Hit **Y** to write out the current mode as **Smart**.  (This writes `type=smart` and triggers a reset flag so the sysmodule uses your new settings.)
- Use **screenoff.nro** whenever you want to turn off the LCD but keep the system awake.  Smart timers will keep counting down; the LED will resume once you wake it up.  It also shows a subtle purple credit line: `made with <3 by neo0oen619`.

**Pro tip:** If you just want simple two‑player multiseat fun, go use Duo.  Wait… wrong project, but the advice stands.

## 🧩 Tech Stack & Build Instructions (Optional)
- Built with **devkitPro/libnx** for the Nintendo Switch.
- The sysmodule lives under `sysmodule/` and compiles to an NSP.  The config app is in `config_app/`, the overlay in `overlay/`, and the screen‑off helper in `screen_off_app/`.
- To build from source, set up devkitA64 and run the appropriate `make` commands for each sub‑project.  See the individual Makefiles or `AGENTS.md` for details.

## 😵 Dark‑ish FAQ
**Q: The LED is blinking like a rave.  Did I just summon something?**  
A: Probably just Smart mode’s idle burst.  Or maybe your battery is at 10% and it’s trying to get your attention.  Check your config or your life choices.

**Q: Why isn’t anything lighting up?**  
A: Make sure you copied `exefs.nsp` to the right title ID and rebooted.  Also check that the LED mode isn’t set to **Off** (we’ve all done it).  If using Smart, ensure `type=smart` in `settings.cfg`.

**Q: My Switch goes to sleep before the LED stops blinking.**  
A: Use the `screenoff.nro` helper with **Smart + Full timeout: Sleep**.  It coordinates with the sysmodule to request a real sleep right when the full‑timeout expires.  Otherwise, the console’s normal idle timer will take over.

**Q: Is this safe?**  
A: As safe as hooking into Horizon and twiddling a hardware LED can be.  It’s been tested on unpatched units and hasn’t bricked anything yet.  Backups and good Karma are still recommended.

**Q: Can I make it strobe like a disco?**  
A: Not yet.  You could implement your own pattern and submit a PR.  Be warned: I will judge your taste in light shows.

## 🤝 Contributing
Pull requests and issues are welcome!

- Open an issue if you find a bug, have a feature request, or your LED is doing something cursed.  Include logs if you have them.
- Fork and submit a PR if you add new modes, improve Smart logic, or make the screen‑off helper less janky.
- Be nice.  Don’t sell the LED patterns on eBay.  Hydrate and call your mom.

## 📜 License
MIT.  Do what you want, but don’t blame me if your Switch decides to start blinking Morse code at 3 AM.  See [LICENSE](LICENSE) for the legally binding stuff.
