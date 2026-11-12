#include "scroller.h"

#include <linux/input.h>

#include <algorithm>
#include <cmath>

namespace {
constexpr int kToolButtons[] = {BTN_TOOL_FINGER, BTN_TOOL_DOUBLETAP, BTN_TOOL_TRIPLETAP,
                                BTN_TOOL_QUADTAP, BTN_TOOL_QUINTTAP};
constexpr int kHiresPerNotch = 120;

// Cruising: a finger within kDeadMm of where it landed does nothing, so a small
// adjustment never starts a runaway scroll. Beyond that, every mm of distance
// adds kCruiseGain notches per second per "notch worth" of travel (so the speed
// setting still means something), up to a ceiling that keeps it controllable.
constexpr double kDeadMm = 4.0;
constexpr double kCruiseGain = 2.0;
constexpr double kMaxNotchesPerSecond = 60.0;
}

Scroller::Scroller(EventSink &touchOut, EventSink &scrollOut, AxisInfo x, AxisInfo y,
                   ScrollOptions options)
    : m_touch(touchOut), m_scroll(scrollOut), m_xInfo(x), m_yInfo(y)
{
    setOptions(options);
}

void Scroller::setOptions(const ScrollOptions &o)
{
    // Without a resolution the best we can do is assume the pad is about 100 mm wide.
    m_xPerMm = m_xInfo.resolution ? m_xInfo.resolution : (m_xInfo.max - m_xInfo.min) / 100.0;
    if (m_xPerMm <= 0)
        m_xPerMm = 1;
    m_strip = m_yInfo.resolution ? o.stripMm * m_yInfo.resolution
                                 : 0.12 * (m_yInfo.max - m_yInfo.min);
    m_notchesPerMm = 1.0 / std::max(0.1, o.mmPerNotch);
    m_hiresPerMm = kHiresPerNotch * m_notchesPerMm;
    m_sign = o.invert ? -1 : 1;
    m_cruise = o.cruise;
}

double Scroller::extentMm(const AxisInfo &a)
{
    return a.resolution ? double(a.max - a.min) / a.resolution : 0.0;
}

void Scroller::send(int type, int code, int value)
{
    m_touch.write(type, code, value);
    m_dirty = true;
}

void Scroller::sendAbs(int slot, int code, int value)
{
    if (m_outSlot != slot) {
        send(EV_ABS, ABS_MT_SLOT, slot);
        m_outSlot = slot;
    }
    send(EV_ABS, code, value);
}

void Scroller::handle(int type, int code, int value)
{
    switch (type) {
    case EV_SYN:
        if (code == SYN_REPORT)
            endFrame();
        else if (code == SYN_DROPPED)
            reset();
        break;
    case EV_ABS:
        onAbs(code, value);
        break;
    case EV_KEY:
        // Touch and tool buttons are recomputed from the slots we forward.
        if (code == BTN_TOUCH || std::find(std::begin(kToolButtons), std::end(kToolButtons), code)
                                     != std::end(kToolButtons))
            return;
        send(EV_KEY, code, value);   // physical clicks
        break;
    case EV_MSC:
        send(EV_MSC, code, value);
        break;
    }
}

void Scroller::onAbs(int code, int value)
{
    if (code == ABS_MT_SLOT) {
        m_cur = value;
        return;
    }
    if (code < ABS_MT_SLOT)
        return;   // ABS_X/ABS_Y mirror the first slot; libinput uses the MT data

    if (code == ABS_MT_TRACKING_ID) {
        if (value == -1) {
            auto it = m_slots.find(m_cur);
            if (it == m_slots.end())
                return;
            Slot s = std::move(it->second);
            m_slots.erase(it);
            if (s.state == State::Pending)
                flush(m_cur, s);   // a quick tap that never left pending still reaches the desktop
            if (s.state != State::Captured)
                sendAbs(m_cur, ABS_MT_TRACKING_ID, -1);
        } else {
            Slot s;
            s.buffer.emplace_back(ABS_MT_TRACKING_ID, value);
            m_slots[m_cur] = std::move(s);
        }
        return;
    }

    auto it = m_slots.find(m_cur);
    if (it == m_slots.end())
        return;
    Slot &s = it->second;
    if (code == ABS_MT_POSITION_X) {
        if (s.state == State::Captured && s.hasX)
            m_dx += value - s.x;
        s.x = value;
        s.hasX = true;
    } else if (code == ABS_MT_POSITION_Y) {
        s.y = value;
        s.hasY = true;
    }
    if (s.state == State::Pending)
        s.buffer.emplace_back(code, value);
    else if (s.state == State::Forwarded)
        sendAbs(m_cur, code, value);
}

void Scroller::flush(int slot, Slot &s)
{
    s.state = State::Forwarded;
    for (const auto &[code, value] : s.buffer)
        sendAbs(slot, code, value);
    s.buffer.clear();
}

void Scroller::endFrame()
{
    for (auto &[slot, s] : m_slots) {
        if (s.state != State::Pending || !s.hasX || !s.hasY)
            continue;
        const bool alone = std::none_of(m_slots.begin(), m_slots.end(), [](const auto &t) {
            return t.second.state == State::Forwarded;
        });
        if (alone && s.y - m_yInfo.min <= m_strip) {
            s.state = State::Captured;
            s.anchorX = s.x;
            s.buffer.clear();
        } else {
            flush(slot, s);
        }
    }

    const int n = int(std::count_if(m_slots.begin(), m_slots.end(), [](const auto &t) {
        return t.second.state == State::Forwarded;
    }));
    if (n != m_forwarded) {
        m_forwarded = n;
        send(EV_KEY, BTN_TOUCH, n ? 1 : 0);
        for (int i = 0; i < 5; ++i)
            send(EV_KEY, kToolButtons[i], std::min(n, 5) == i + 1 ? 1 : 0);
    }

    if (m_dirty) {
        m_touch.syn();
        m_dirty = false;
    }
    scroll();
}

void Scroller::scroll()
{
    if (!m_dx)
        return;
    const double mm = m_dx / m_xPerMm;
    m_dx = 0;
    addHires(mm * m_hiresPerMm * m_sign);
}

double Scroller::cruiseRate() const
{
    if (!m_cruise)
        return 0;
    // With two fingers in the strip, the one that has slid furthest decides.
    double best = 0;
    for (const auto &[slot, s] : m_slots) {
        if (s.state != State::Captured || !s.hasX)
            continue;
        const double mm = (s.x - s.anchorX) / m_xPerMm;
        const double beyond = std::abs(mm) - kDeadMm;
        if (beyond <= 0)
            continue;
        const double rate = std::min(kMaxNotchesPerSecond, beyond * m_notchesPerMm * kCruiseGain);
        if (rate > std::abs(best))
            best = mm < 0 ? -rate : rate;
    }
    return best * m_sign;
}

void Scroller::tick(double seconds)
{
    const double rate = cruiseRate();
    if (rate == 0 || seconds <= 0)
        return;
    addHires(rate * kHiresPerNotch * seconds);
}

void Scroller::addHires(double hires)
{
    m_hiresAcc += hires;
    const int whole = int(m_hiresAcc);   // truncates toward zero; the remainder carries over
    if (!whole)
        return;
    m_hiresAcc -= whole;
    m_scroll.write(EV_REL, REL_WHEEL_HI_RES, whole);
    m_notchAcc += whole;
    const int notches = m_notchAcc / kHiresPerNotch;
    if (notches) {
        m_notchAcc -= notches * kHiresPerNotch;
        m_scroll.write(EV_REL, REL_WHEEL, notches);
    }
    m_scroll.syn();
}

void Scroller::reset()
{
    // The kernel dropped events: release everything we forwarded.
    for (const auto &[slot, s] : m_slots)
        if (s.state == State::Forwarded)
            sendAbs(slot, ABS_MT_TRACKING_ID, -1);
    m_slots.clear();
    m_forwarded = 0;
    send(EV_KEY, BTN_TOUCH, 0);
    for (int btn : kToolButtons)
        send(EV_KEY, btn, 0);
    m_touch.syn();
    m_dirty = false;
}
