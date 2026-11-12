#pragma once

#include <linux/input.h>

#include <bitset>
#include <string>

#include <QString>

#include "eventsink.h"

// What a kernel input device can do, as much as we need to clone a touchpad.
struct DeviceCaps {
    std::string name;
    input_id id{};
    std::bitset<EV_MAX + 1> ev;
    std::bitset<KEY_MAX + 1> key;
    std::bitset<ABS_MAX + 1> abs;
    std::bitset<MSC_MAX + 1> msc;
    std::bitset<INPUT_PROP_MAX + 1> props;
    input_absinfo absinfo[ABS_MAX + 1]{};

    bool isTouchpad() const;
};

// Reads the capabilities of an open evdev node. Returns false if it isn't one.
bool readCaps(int fd, DeviceCaps &caps);

// A virtual input device created through /dev/uinput. Destroying it releases
// every touch or button it still holds down.
class UInputDevice : public EventSink {
public:
    UInputDevice() = default;
    ~UInputDevice() override;
    UInputDevice(const UInputDevice &) = delete;
    UInputDevice &operator=(const UInputDevice &) = delete;

    // A virtual twin of `source` that the compositor treats as another touchpad.
    bool createClone(const DeviceCaps &source, const std::string &name);
    // A virtual mouse wheel.
    bool createWheel(const std::string &name);

    void write(int type, int code, int value) override;
    void syn() override;

    int lastError() const { return m_errno; }

private:
    bool open();
    bool finish(const std::string &name, const input_id &id);

    int m_fd = -1;
    int m_errno = 0;
};
