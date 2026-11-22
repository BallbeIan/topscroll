#pragma once

#include <QString>

#include <memory>
#include <vector>

#include "scroller.h"

// One finger as a Windows Precision Touchpad reports it.
struct TouchContact {
    int id = 0;
    bool tip = false;   // touching (and, when the pad says so, not a palm)
    int x = 0;          // in the scaled units of TouchpadParser::x/y
    int y = 0;
};

// Reads the HID input reports of a Windows Precision Touchpad (usage page 0x0D,
// usage 0x05) as delivered through Raw Input. The report layout comes from the
// device's preparsed HID data, so any spec-conforming touchpad works.
class TouchpadParser {
public:
    // Returns null if `device` (a Raw Input HANDLE) isn't a usable precision touchpad.
    static std::unique_ptr<TouchpadParser> open(void *device);

    void *device() const { return m_device; }
    QString name() const { return m_name; }
    // Coordinates are scaled up by kScale so the resolution, which AxisInfo
    // stores as whole units per mm, keeps its precision.
    AxisInfo x() const { return m_x; }
    AxisInfo y() const { return m_y; }

    // Feeds one HID report. A touchpad may split a frame across several reports
    // ("hybrid mode"); returns true and fills `frame` once a whole frame is in.
    bool feed(const unsigned char *report, unsigned long size, std::vector<TouchContact> &frame);

    static constexpr int kScale = 16;

private:
    TouchpadParser() = default;
    bool readContact(unsigned short link, int index, char *report, unsigned long size,
                     TouchContact &out) const;

    void *m_device = nullptr;
    QString m_name;
    AxisInfo m_x, m_y;
    std::vector<char> m_preparsed;
    std::vector<unsigned short> m_fingers;   // link collections that carry one finger each
    bool m_hasCount = false;
    unsigned short m_countLink = 0;
    bool m_hasConfidence = false;
    unsigned long m_maxUsages = 0;

    std::vector<TouchContact> m_pending;
    int m_remaining = 0;
};
