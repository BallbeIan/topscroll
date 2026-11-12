# topscroll

Scroll with **one finger** on a Linux laptop touchpad by sliding along its **top edge**.

- Slide **right** along the top edge → scroll **up**
- Slide **left** along the top edge → scroll **down**

The rest of the touchpad (cursor, taps, clicks, multi-finger gestures) keeps working as normal.

## How it works

libinput has no top-edge scrolling, so `topscroll` grabs the touchpad through evdev and re-emits
every touch through a virtual touchpad. A touch that *starts* in the top strip while no other finger
is down is swallowed (the cursor doesn't move) and its horizontal motion is turned into wheel events
on a virtual mouse (`REL_WHEEL` / `REL_WHEEL_HI_RES`). It works on Wayland and X11 because it sits
below the compositor.

## Requirements

- Linux with evdev/uinput
- Python 3 and [`python-evdev`](https://python-evdev.readthedocs.io/) (`pacman -S python-evdev` on Arch)
- Read access to `/dev/input/event*` (group `input`) and write access to `/dev/uinput`:

```sh
sudo usermod -aG input $USER   # then log out of your desktop session and back in
```

## Usage

```sh
python3 topscroll.py
```

Press Ctrl+C to stop; the touchpad returns to normal (the kernel also releases the grab if the
process dies).

| Option | Default | Meaning |
| --- | --- | --- |
| `--device PATH` | auto-detect | touchpad evdev node, e.g. `/dev/input/event7` |
| `--strip-mm N` | `12` | height of the scroll strip along the top edge |
| `--mm-per-notch N` | `5` | finger travel per wheel notch (smaller = faster) |
| `--invert` | off | swap direction |

## Known limitations

- Per-device compositor settings keyed on the touchpad's name won't apply to the virtual device,
  which is named `<touchpad name> (topscroll)`. Global touchpad settings still apply.
- Only the first detected touchpad is handled, and there is no hotplug support yet.

## Roadmap

- [ ] systemd user service / autostart
- [ ] Config file
- [ ] Scroll acceleration and inertia
- [ ] Horizontal scrolling option
- [ ] Packaging (PKGBUILD / AUR)
- [ ] Optional settings UI / tray icon

## License

MIT
