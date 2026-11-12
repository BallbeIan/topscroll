<div align="center">

# topscroll

**Scroll with one finger.**
Slide along the top edge of your laptop's touchpad to scroll, in any app. Everything else on the touchpad works exactly as before.

![Linux](https://img.shields.io/badge/Linux-Wayland%20%26%20X11-FCC624?logo=linux&logoColor=black)
![Made for Omarchy](https://img.shields.io/badge/made%20for-Omarchy-1f6feb)
![License: MIT](https://img.shields.io/badge/license-MIT-blue)

<img src="docs/images/settings.png" alt="The topscroll window: an animated touchpad showing the gesture, a big on/off switch, simple Speed and Scroll area sliders, switches for smooth stop, hold to keep scrolling and flip direction, and a collapsed Fine-tune section" width="360">

</div>

---

## What it does

Put one finger on the **top edge** of your touchpad and slide.

- Slide **right** to scroll **up**.
- Slide **left** to scroll **down**.

That's it. No second finger, no keyboard shortcut. Moving the cursor, tapping, clicking and multi-finger gestures all work the way they always did, because only touches that start on that top strip are treated as scrolling.

## Nice touches

- **Smooth stop.** Lift your finger mid-slide and the scrolling coasts on and eases out, like flicking a wheel.
- **Hold to keep scrolling.** Optional. Keep your finger down and it keeps going, faster the further you slide. Small accidental movements are ignored.
- **Make it yours.** Change the speed, the size of the strip and the direction, and watch the preview react. Every change applies instantly.
- **Always on.** It runs quietly in the background and comes back every time you log in. Closing the window doesn't stop it.
- **Private.** No network access, no logging, no analytics.

## Install

topscroll is made for [Omarchy](https://omarchy.org) and isn't in its package list yet, so you build the package once. You'll need `git` and `base-devel`; the Qt libraries it needs come with Omarchy.

```bash
git clone https://github.com/BallbeIan/topscroll.git
cd topscroll
bin/install
```

It will ask for your password to install the package. Then:

1. Open **topscroll** from the app launcher (`Super + Space`).
2. Press **Start** if it asks you to.
3. Slide a finger along the top edge of your touchpad.

If it says **Needs permission**, log out and back in once and try again.

<details>
<summary>Uninstall</summary>

```bash
systemctl --user disable --now topscroll
sudo pacman -R topscroll
```

</details>

## Using it

Everything is in the window. The sliders and switches do what they say:

| Setting | What it does |
|---|---|
| **On / off switch** | Turns scrolling off completely, so your touchpad behaves as if topscroll weren't installed. |
| **Speed** | How far you slide for each scroll step, from slow to fast. |
| **Scroll area** | How tall the scrolling strip along the top edge is. |
| **Smooth stop** | Keep coasting after you lift your finger, then ease to a stop. |
| **Hold to keep scrolling** | Keep your finger down and it keeps scrolling. The further you slide from where you started, the faster it goes. |
| **Flip direction** | Slide right to scroll down instead of up. |
| **Fine-tune** | Opens the finer controls: how long the smooth stop lasts, how much movement is ignored, and the start and top speed for holding to keep scrolling. **Reset to defaults** is in here too. |

Press `?` in the window for keyboard shortcuts.

<div align="center">
<img src="docs/images/shortcuts.png" alt="The keyboard shortcuts overlay" width="300">
</div>

## Good to know

- **It's new.** topscroll was built and tested on one Omarchy laptop. Other touchpads and desktops should work but haven't been verified. If you try it, please [tell me how it went](../../issues/new/choose), even if everything works.
- **One touchpad.** Only the first touchpad it finds is used.
- **Permissions.** To see your touches and create a virtual touchpad, topscroll gets access to your touchpad and to `/dev/uinput`, which the package sets up for you. Any program you run can use `/dev/uinput` to create virtual input devices, including a virtual keyboard; other packages such as game-controller remappers make the same trade-off. If that's not for you, don't install topscroll.
- **Name-based settings.** The virtual touchpad is named `<your touchpad> (topscroll)`, so desktop settings keyed on the touchpad's exact name won't apply to it. General touchpad settings still do.

## Something not working?

| You see | Try this |
|---|---|
| **Needs permission** | Log out and back in, then open topscroll again. |
| **Can't start** | Log out and back in, or run `sudo modprobe uinput`. |
| **Touchpad in use** | Another program has taken over the touchpad. Close it and try again. |
| **Not running** | Press **Start**. |
| **Update ready** | Press **Restart** to start using the new version. |
| Scrolls the wrong way | Turn on **Flip direction**. |

Still stuck? [Open an issue](../../issues/new/choose) and include the output of `topscroll status --json`.

## For developers

How it works, the code layout, building from source and adding a translation are in [docs/DEVELOPING.md](docs/DEVELOPING.md). Contributions are welcome; see [CONTRIBUTING.md](CONTRIBUTING.md). Command line: `topscroll` opens the window, `topscroll daemon` runs the service in the foreground, and `topscroll status` reports what the service is doing.

Available in English, Danish, German, Spanish and French.

## License

[MIT](LICENSE)
