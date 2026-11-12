#!/usr/bin/env python3
"""topscroll - scroll by sliding one finger along the top edge of the touchpad.

Slide right along the top edge -> scroll up, slide left -> scroll down.

How it works: the real touchpad is grabbed exclusively and every touch is
re-emitted through a virtual touchpad (so normal cursor movement, taps and
gestures keep working via libinput/the compositor). A touch that *starts* in
the top strip while no other finger is down is swallowed instead: its
horizontal movement is turned into wheel events on a virtual mouse.

Needs read access to /dev/input/event* (group `input`) and write access to
/dev/uinput. Stop with Ctrl+C; the kernel releases the grab when we exit.
"""
import argparse
import glob
import sys

from evdev import InputDevice, UInput, ecodes as e

TOOL_BTNS = [e.BTN_TOOL_FINGER, e.BTN_TOOL_DOUBLETAP, e.BTN_TOOL_TRIPLETAP,
             e.BTN_TOOL_QUADTAP, e.BTN_TOOL_QUINTTAP]
HIRES_PER_NOTCH = 120

PENDING, FWD, CAP = "pending", "fwd", "cap"


class Slot:
    def __init__(self):
        self.state = PENDING
        self.buf = []          # events held back until we know where the touch began
        self.x = None
        self.y = None


class TopEdgeScroller:
    def __init__(self, touch_out, scroll_out, x_info, y_info,
                 strip_mm=12.0, mm_per_notch=5.0, invert=False):
        self.touch_out = touch_out
        self.scroll_out = scroll_out
        self.y_min = y_info.min
        # Touchpad resolution is units/mm; fall back to a fraction of the range.
        self.x_res = x_info.resolution or (x_info.max - x_info.min) / 100.0
        if y_info.resolution:
            self.strip = strip_mm * y_info.resolution
        else:
            self.strip = 0.12 * (y_info.max - y_info.min)
        self.hires_per_mm = HIRES_PER_NOTCH / mm_per_notch
        self.sign = -1 if invert else 1   # right = up (positive wheel) by default

        self.slots = {}
        self.cur = 0
        self.out_slot = None
        self.n_fwd = 0
        self.dx = 0                 # captured horizontal movement this frame
        self.hires_acc = 0.0
        self.notch_acc = 0
        self.dirty = False

    # --- output helpers -------------------------------------------------
    def _emit(self, etype, code, value):
        self.touch_out.write(etype, code, value)
        self.dirty = True

    def _emit_abs(self, slot, code, value):
        if self.out_slot != slot:
            self._emit(e.EV_ABS, e.ABS_MT_SLOT, slot)
            self.out_slot = slot
        self._emit(e.EV_ABS, code, value)

    # --- input ----------------------------------------------------------
    def handle(self, ev):
        if ev.type == e.EV_SYN:
            if ev.code == e.SYN_REPORT:
                self._end_frame()
            elif ev.code == e.SYN_DROPPED:
                self._reset()
        elif ev.type == e.EV_ABS:
            self._abs(ev.code, ev.value)
        elif ev.type == e.EV_KEY:
            if ev.code == e.BTN_TOUCH or ev.code in TOOL_BTNS:
                return              # recomputed from the slots we forward
            self._emit(e.EV_KEY, ev.code, ev.value)   # physical clicks
        elif ev.type == e.EV_MSC:
            self._emit(e.EV_MSC, ev.code, ev.value)

    def _abs(self, code, value):
        if code == e.ABS_MT_SLOT:
            self.cur = value
            return
        if code < e.ABS_MT_SLOT:
            return                  # ABS_X/ABS_Y mirror; libinput uses MT data
        if code == e.ABS_MT_TRACKING_ID:
            if value == -1:
                s = self.slots.pop(self.cur, None)
                if s is None:
                    return
                if s.state == PENDING:
                    self._flush(self.cur, s)
                if s.state != CAP:
                    self._emit_abs(self.cur, e.ABS_MT_TRACKING_ID, -1)
            else:
                s = Slot()
                s.buf.append((e.ABS_MT_TRACKING_ID, value))
                self.slots[self.cur] = s
            return
        s = self.slots.get(self.cur)
        if s is None:
            return
        if code == e.ABS_MT_POSITION_X:
            if s.state == CAP and s.x is not None:
                self.dx += value - s.x
            s.x = value
        elif code == e.ABS_MT_POSITION_Y:
            s.y = value
        if s.state == PENDING:
            s.buf.append((code, value))
        elif s.state == FWD:
            self._emit_abs(self.cur, code, value)

    def _flush(self, slot, s):
        s.state = FWD
        for code, value in s.buf:
            self._emit_abs(slot, code, value)
        s.buf = []

    def _end_frame(self):
        for slot, s in list(self.slots.items()):
            if s.state != PENDING or s.x is None or s.y is None:
                continue
            alone = all(t.state != FWD for t in self.slots.values())
            if alone and s.y - self.y_min <= self.strip:
                s.state = CAP
                s.buf = []
            else:
                self._flush(slot, s)

        n = sum(1 for s in self.slots.values() if s.state == FWD)
        if n != self.n_fwd:
            self.n_fwd = n
            self._emit(e.EV_KEY, e.BTN_TOUCH, 1 if n else 0)
            for i, btn in enumerate(TOOL_BTNS):
                self._emit(e.EV_KEY, btn, 1 if min(n, 5) == i + 1 else 0)

        if self.dirty:
            self.touch_out.syn()
            self.dirty = False
        self._scroll()

    def _scroll(self):
        if not self.dx:
            return
        mm = self.dx / self.x_res
        self.dx = 0
        self.hires_acc += mm * self.hires_per_mm * self.sign
        whole = int(self.hires_acc)
        if not whole:
            return
        self.hires_acc -= whole
        self.scroll_out.write(e.EV_REL, e.REL_WHEEL_HI_RES, whole)
        self.notch_acc += whole
        notches = int(self.notch_acc / HIRES_PER_NOTCH)
        if notches:
            self.notch_acc -= notches * HIRES_PER_NOTCH
            self.scroll_out.write(e.EV_REL, e.REL_WHEEL, notches)
        self.scroll_out.syn()

    def _reset(self):
        """Kernel dropped events: release everything we forwarded."""
        for slot, s in self.slots.items():
            if s.state == FWD:
                self._emit_abs(slot, e.ABS_MT_TRACKING_ID, -1)
        self.slots.clear()
        self.n_fwd = 0
        self._emit(e.EV_KEY, e.BTN_TOUCH, 0)
        for btn in TOOL_BTNS:
            self._emit(e.EV_KEY, btn, 0)
        self.touch_out.syn()
        self.dirty = False


def find_touchpad(path=None):
    if path:
        return InputDevice(path)
    denied = False
    # (evdev.list_devices() hides unreadable nodes, which would mask a permission problem)
    for p in sorted(glob.glob("/dev/input/event*")):
        try:
            d = InputDevice(p)
        except PermissionError:
            denied = True
            continue
        caps = d.capabilities()
        if (e.ABS_MT_POSITION_X in [c for c, _ in caps.get(e.EV_ABS, [])]
                and e.BTN_TOOL_FINGER in caps.get(e.EV_KEY, [])
                and "touchpad" in d.name.lower()):
            return d
    if denied:
        sys.exit("Permission denied reading /dev/input/*. Add yourself to the "
                 "`input` group (sudo usermod -aG input $USER) and log in again.")
    sys.exit("No touchpad found (try --device /dev/input/eventN).")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--device", help="touchpad evdev path (auto-detected by default)")
    ap.add_argument("--strip-mm", type=float, default=12.0,
                    help="height of the scroll strip along the top edge (default 12)")
    ap.add_argument("--mm-per-notch", type=float, default=5.0,
                    help="finger travel per wheel notch; smaller = faster (default 5)")
    ap.add_argument("--invert", action="store_true",
                    help="swap direction (right = scroll down)")
    args = ap.parse_args()

    dev = find_touchpad(args.device)
    caps = dev.capabilities(absinfo=True)
    caps.pop(e.EV_SYN, None)
    absinfo = dict(caps[e.EV_ABS])
    x_info, y_info = absinfo[e.ABS_MT_POSITION_X], absinfo[e.ABS_MT_POSITION_Y]

    touch_out = UInput(caps, name=dev.name + " (topscroll)",
                       vendor=dev.info.vendor, product=dev.info.product,
                       version=dev.info.version, bustype=dev.info.bustype,
                       input_props=dev.input_props())
    scroll_out = UInput({e.EV_REL: [e.REL_WHEEL, e.REL_WHEEL_HI_RES],
                         e.EV_KEY: [e.BTN_LEFT]},
                        name="topscroll wheel")
    router = TopEdgeScroller(touch_out, scroll_out, x_info, y_info,
                             args.strip_mm, args.mm_per_notch, args.invert)

    print(f"topscroll: {dev.name} ({dev.path}); strip {args.strip_mm} mm. Ctrl+C to quit.")
    dev.grab()
    try:
        for ev in dev.read_loop():
            router.handle(ev)
    except KeyboardInterrupt:
        pass
    finally:
        try:
            dev.ungrab()
        except OSError:
            pass
        touch_out.close()
        scroll_out.close()


if __name__ == "__main__":
    main()
