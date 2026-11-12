# Developing topscroll

Everything under the hood: how it works, how the code is laid out, how to build it, and how to add a translation. If you just want to use topscroll, the [README](../README.md) is all you need.

## How it works

libinput has no top-edge scrolling, so the daemon grabs the touchpad through evdev and re-emits every touch through a virtual touchpad. A touch that *starts* in the top strip while no other finger is down is swallowed, so the cursor does not move, and its horizontal motion becomes wheel events on a virtual mouse. It works on Wayland and X11 because it sits below the compositor.

```
 touchpad ──► Daemon ──► Scroller ──┬──► virtual touchpad   (every other touch, clicks, gestures)
 (grabbed)      ▲                   └──► virtual wheel      (touches that began in the strip)
                │
 topscroll.conf ┘   settings window ──► topscroll.conf       daemon ──► status.json ──► settings window
```

The settings window only edits `topscroll.conf`, and the daemon watches that file. The daemon reports what it is doing in `$XDG_RUNTIME_DIR/topscroll/status.json`, which the window and `topscroll status` read.

| Path | What it does |
|---|---|
| `src/scroller.{h,cpp}` | The routing logic. Writes to an `EventSink`, so tests need no hardware |
| `src/evdev.{h,cpp}` | Reads touchpad capabilities and creates virtual devices through `/dev/uinput` |
| `src/daemon.{h,cpp}` | Finds and grabs the touchpad, follows the config file, publishes status |
| `src/config.*`, `src/status.*` | The settings file and the status file |
| `src/backend.*`, `src/Main.qml` | The settings window |
| `src/theme.*`, `src/translator.*`, `src/i18n/` | Omarchy theme and language, followed live |
| `pkgbuild/` | PKGBUILD, desktop entry, icon, udev rule, user service |
| `tests/` | Qt Test suite |

## Config file

Settings live in `~/.config/topscroll/topscroll.conf` (INI). The window writes it as you change things and the daemon applies changes immediately. Values outside the ranges are clamped.

| Key | Default | Range | Meaning |
|---|---|---|---|
| `enabled` | `true` | | Let go of the touchpad entirely when `false` |
| `strip_mm` | `12` | 4 to 30 | Height of the scroll strip |
| `mm_per_notch` | `5` | 2 to 12 | Finger travel per wheel notch |
| `invert` | `false` | | Right scrolls down |
| `inertia` | `true` | | Keep gliding after the finger lifts |
| `glide_seconds` | `1.0` | 0.2 to 3 | Roughly how long a glide lasts |
| `cruise` | `false` | | Hold to keep scrolling |
| `dead_mm` | `1.5` | 0.5 to 8 | Cruise: movement ignored around where the finger landed |
| `cruise_start` | `2.0` | 0.5 to 10 | Cruise: notches per second at the start |
| `cruise_max` | `60` | 10 to 120 | Cruise: top speed in notches per second |

## Building from source

You need `qmake6`, `make`, a C++20 compiler and Qt 6 base and declarative. On Omarchy:

```bash
omarchy-pkg-add base-devel qt6-base qt6-declarative qt6-wayland
```

```bash
git clone https://github.com/BallbeIan/topscroll.git
cd topscroll
bin/build          # qmake6 + make into build/topscroll
bin/test           # Qt Test suite, offscreen, never touches real input devices
bin/screenshot docs/images/settings.png   # regenerate the README screenshot from the real app (QT_SCALE_FACTOR=2 for sharper)
```

When trying `build/topscroll daemon` by hand, point `XDG_CONFIG_HOME` and `XDG_RUNTIME_DIR` at a throwaway directory so it does not touch your real settings.

## Translations

English, Danish, German, Spanish and French so far. Other languages show English. Translations are small JSON files in [`src/i18n/`](src/i18n) (English text to translation), so adding a language is one file plus a line in `src/resources.qrc`.

## Regenerating the screenshots

The README images come from the real app, not a mock-up:

```bash
bin/build
QT_SCALE_FACTOR=2 bin/screenshot docs/images/settings.png 440x1040
QT_SCALE_FACTOR=2 bin/screenshot docs/images/shortcuts.png 440x700 help
```

It runs offscreen with a throwaway config, so it never touches your settings or your desktop.
