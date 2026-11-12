#pragma once

#include <map>
#include <utility>
#include <vector>

#include "eventsink.h"

struct AxisInfo {
    int min = 0;
    int max = 0;
    int resolution = 0;   // units per mm, 0 when the kernel doesn't know
};

struct ScrollOptions {
    double stripMm = 12.0;      // height of the scroll strip along the top edge
    double mmPerNotch = 5.0;    // finger travel per wheel notch; smaller is faster
    bool invert = false;        // swap direction
    // Keep scrolling while a finger rests on the strip, faster the further it
    // has slid from where it touched down. Lifting the finger stops it.
    bool cruise = false;
    double deadMm = 1.5;          // cruise: movement this close to where the finger landed is ignored
    double cruiseStart = 2.0;     // cruise: notches per second the moment it leaves the dead zone
    double cruiseMax = 60.0;      // cruise: speed ceiling, in notches per second
    // Keep scrolling after the finger lifts, easing to a stop over roughly
    // `glideSeconds`. Applies to both normal scrolling and cruise.
    bool inertia = true;
    double glideSeconds = 1.0;
};

// Routes touchpad events. Every touch is re-emitted through `touchOut`, except
// one that *starts* in the top strip while no other finger is down: that one is
// swallowed and its horizontal movement becomes wheel events on `scrollOut`.
// Right along the strip scrolls up, left scrolls down.
class Scroller {
public:
    Scroller(EventSink &touchOut, EventSink &scrollOut, AxisInfo x, AxisInfo y,
             ScrollOptions options);

    void setOptions(const ScrollOptions &options);
    void handle(int type, int code, int value);

    // Advances cruising by `seconds`. The caller supplies the clock, so tests
    // need no real time and the daemon only ticks while there is something to do.
    void tick(double seconds);
    // True while a finger in the strip is far enough from where it landed to scroll.
    bool cruising() const { return cruiseRate() != 0; }
    // True when the caller should keep calling tick(): a finger is cruising, a
    // finger is sliding (so its speed can be measured) or a glide is under way.
    bool needsTick() const;
    // Cruise speed in wheel notches per second, signed like the wheel (positive is up).
    double cruiseRate() const;

    // Physical size of an axis in mm, or 0 when the device doesn't report it.
    static double extentMm(const AxisInfo &axis);

private:
    enum class State { Pending, Forwarded, Captured };

    struct Slot {
        State state = State::Pending;
        std::vector<std::pair<int, int>> buffer;   // held back until we know where the touch began
        bool hasX = false, hasY = false;
        int x = 0, y = 0;
        int anchorX = 0;   // where a captured touch landed; cruise speed grows with distance from it
    };

    void send(int type, int code, int value);
    void sendAbs(int slot, int code, int value);
    void onAbs(int code, int value);
    void flush(int slot, Slot &s);
    void endFrame();
    void scroll();
    void addHires(double hires);
    void reset();
    void beginGlide();
    bool hasCaptured() const;

    EventSink &m_touch;
    EventSink &m_scroll;
    AxisInfo m_xInfo, m_yInfo;
    double m_xPerMm = 1;
    double m_strip = 0;
    double m_hiresPerMm = 24;
    double m_notchesPerMm = 0.2;
    bool m_cruise = false;
    double m_deadMm = 1.5;
    double m_cruiseStart = 2.0;
    double m_cruiseMax = 60.0;
    bool m_inertia = true;
    double m_tau = 1.0 / 3;       // glide decay time constant, seconds
    double m_vel = 0;             // measured speed of a sliding finger, hires units per second
    double m_glide = 0;           // current glide speed, hires units per second
    double m_tickHires = 0;       // scrolled directly since the last tick
    int m_sign = 1;

    std::map<int, Slot> m_slots;
    int m_cur = 0;
    int m_outSlot = -1;
    int m_forwarded = 0;
    int m_dx = 0;                 // captured horizontal movement this frame
    double m_hiresAcc = 0;
    int m_notchAcc = 0;
    bool m_dirty = false;
};
