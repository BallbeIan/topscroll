#pragma once

// Where the Scroller sends events. Production writes to uinput devices; tests
// collect them in memory, so the routing logic never needs real hardware.
class EventSink {
public:
    virtual ~EventSink() = default;
    virtual void write(int type, int code, int value) = 0;
    virtual void syn() = 0;
};
