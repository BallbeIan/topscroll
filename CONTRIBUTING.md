# Contributing

Thanks for your interest in topscroll! Bug reports, device reports and pull requests are welcome.

## Device reports are the most useful thing right now

topscroll has been developed on one Omarchy laptop. How it behaves on other touchpads and desktops is the biggest unknown. If you build it and try it, please open a **Bug report** issue even when everything works, and include:

- laptop model, and the touchpad's name from `libinput list-devices`
- your desktop or compositor, and whether it is Wayland or X11
- the output of `topscroll status --json`
- whether sliding along the top edge scrolls, and whether the cursor, taps, clicks and multi-finger gestures still behave normally

## Building

See [Building from source](README.md#building-from-source). Run `bin/test` before opening a pull request.

## Pull requests

- Keep changes focused, and describe what you tested and on which touchpad.
- Match the existing style: small classes, C++ for logic and QML for layout, no third-party libraries.
- Anything user-visible is wrapped in `qsTr()` or `tr()`. New strings are welcome in the translation files, and English stays the fallback.
- Routing changes in `src/scroller.cpp` need a test in `tests/topscroll_tests.cpp`.

## Reporting security problems

topscroll runs with access to your touchpad and `/dev/uinput`. Please do not open a public issue for a security problem. Use GitHub's private vulnerability reporting for this repository if it is enabled, or contact the maintainer through their GitHub profile.
