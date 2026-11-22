#include "wintouchpad.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <hidsdi.h>
#include <hidpi.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <string>

namespace {

constexpr USHORT kPageDesktop = 0x01;
constexpr USHORT kPageDigitizer = 0x0D;
constexpr USAGE kUsageTouchpad = 0x05;
constexpr USAGE kUsageX = 0x30;
constexpr USAGE kUsageY = 0x31;
constexpr USAGE kUsageTip = 0x42;
constexpr USAGE kUsageConfidence = 0x47;
constexpr USAGE kUsageContactId = 0x51;
constexpr USAGE kUsageContactCount = 0x54;

bool covers(USAGE usage, BOOLEAN isRange, USAGE single, USAGE lo, USAGE hi)
{
    return isRange ? (usage >= lo && usage <= hi) : usage == single;
}

// Logical range plus, when the descriptor declares a physical size, units per mm.
AxisInfo axisFrom(const HIDP_VALUE_CAPS &c)
{
    LONG lo = c.LogicalMin;
    LONG hi = c.LogicalMax;
    if (hi <= lo && c.BitSize > 0 && c.BitSize < 31)   // unsigned maximum read as negative
        hi = (LONG(1) << c.BitSize) - 1;

    AxisInfo a;
    a.min = int(lo) * TouchpadParser::kScale;
    a.max = int(hi) * TouchpadParser::kScale;

    LONG exponent = LONG(c.UnitsExp);
    if (exponent > 7 && exponent < 16)   // a 4-bit two's complement nibble
        exponent -= 16;
    const double physical = double(c.PhysicalMax - c.PhysicalMin) * std::pow(10.0, exponent);
    const ULONG system = c.Units & 0xF;           // 1 = SI linear (cm), 3 = English linear (inch)
    const ULONG length = (c.Units >> 4) & 0xF;    // exponent of the length dimension
    double mm = 0;
    if (length == 1 && system == 1)
        mm = physical * 10.0;
    else if (length == 1 && system == 3)
        mm = physical * 25.4;
    if (mm > 1)
        a.resolution = int(std::lround(double(a.max - a.min) / mm));
    return a;
}

QString productName(HANDLE device)
{
    UINT chars = 0;
    GetRawInputDeviceInfoW(device, RIDI_DEVICENAME, nullptr, &chars);
    if (!chars)
        return {};
    std::wstring path(chars, L'\0');
    if (GetRawInputDeviceInfoW(device, RIDI_DEVICENAME, path.data(), &chars) == UINT(-1))
        return {};
    // Access 0 is enough to ask for strings, and never conflicts with the driver.
    HANDLE h = CreateFileW(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                           OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return {};
    wchar_t buf[128] = {};
    QString name;
    if (HidD_GetProductString(h, buf, sizeof buf))
        name = QString::fromWCharArray(buf).trimmed();
    CloseHandle(h);
    return name;
}

}

std::unique_ptr<TouchpadParser> TouchpadParser::open(void *device)
{
    RID_DEVICE_INFO info{};
    info.cbSize = sizeof info;
    UINT size = sizeof info;
    if (GetRawInputDeviceInfoW(device, RIDI_DEVICEINFO, &info, &size) == UINT(-1))
        return {};
    if (info.dwType != RIM_TYPEHID || info.hid.usUsagePage != kPageDigitizer
        || info.hid.usUsage != kUsageTouchpad)
        return {};

    std::unique_ptr<TouchpadParser> p(new TouchpadParser);
    p->m_device = device;

    UINT ppSize = 0;
    GetRawInputDeviceInfoW(device, RIDI_PREPARSEDDATA, nullptr, &ppSize);
    if (!ppSize)
        return {};
    p->m_preparsed.resize(ppSize);
    if (GetRawInputDeviceInfoW(device, RIDI_PREPARSEDDATA, p->m_preparsed.data(), &ppSize) == UINT(-1))
        return {};
    const auto pp = reinterpret_cast<PHIDP_PREPARSED_DATA>(p->m_preparsed.data());

    HIDP_CAPS caps{};
    if (HidP_GetCaps(pp, &caps) != HIDP_STATUS_SUCCESS)
        return {};

    std::vector<HIDP_VALUE_CAPS> values(caps.NumberInputValueCaps);
    USHORT nValues = caps.NumberInputValueCaps;
    if (nValues && HidP_GetValueCaps(HidP_Input, values.data(), &nValues, pp) != HIDP_STATUS_SUCCESS)
        return {};
    values.resize(nValues);

    // Each finger is a logical collection holding its own X and Y.
    std::map<USHORT, std::pair<const HIDP_VALUE_CAPS *, const HIDP_VALUE_CAPS *>> axes;
    for (const HIDP_VALUE_CAPS &v : values) {
        const USAGE single = v.IsRange ? 0 : v.NotRange.Usage;
        const USAGE lo = v.IsRange ? v.Range.UsageMin : 0;
        const USAGE hi = v.IsRange ? v.Range.UsageMax : 0;
        if (v.UsagePage == kPageDesktop && covers(kUsageX, v.IsRange, single, lo, hi))
            axes[v.LinkCollection].first = &v;
        else if (v.UsagePage == kPageDesktop && covers(kUsageY, v.IsRange, single, lo, hi))
            axes[v.LinkCollection].second = &v;
        else if (v.UsagePage == kPageDigitizer && covers(kUsageContactCount, v.IsRange, single, lo, hi)) {
            p->m_hasCount = true;
            p->m_countLink = v.LinkCollection;
        }
    }
    for (const auto &[link, xy] : axes) {
        if (!xy.first || !xy.second)
            continue;
        if (p->m_fingers.empty()) {
            p->m_x = axisFrom(*xy.first);
            p->m_y = axisFrom(*xy.second);
        }
        p->m_fingers.push_back(link);
    }
    if (p->m_fingers.empty() || p->m_x.max <= p->m_x.min || p->m_y.max <= p->m_y.min)
        return {};

    std::vector<HIDP_BUTTON_CAPS> buttons(caps.NumberInputButtonCaps);
    USHORT nButtons = caps.NumberInputButtonCaps;
    if (nButtons && HidP_GetButtonCaps(HidP_Input, buttons.data(), &nButtons, pp) == HIDP_STATUS_SUCCESS) {
        for (USHORT i = 0; i < nButtons; ++i) {
            const HIDP_BUTTON_CAPS &b = buttons[i];
            if (b.UsagePage == kPageDigitizer
                && covers(kUsageConfidence, b.IsRange, b.IsRange ? 0 : b.NotRange.Usage,
                          b.IsRange ? b.Range.UsageMin : 0, b.IsRange ? b.Range.UsageMax : 0))
                p->m_hasConfidence = true;
        }
    }
    p->m_maxUsages = HidP_MaxUsageListLength(HidP_Input, kPageDigitizer, pp);

    p->m_name = productName(device);
    if (p->m_name.isEmpty())
        p->m_name = QStringLiteral("Precision touchpad");
    return p;
}

bool TouchpadParser::readContact(unsigned short link, int index, char *report, unsigned long size,
                                 TouchContact &out) const
{
    const auto pp = reinterpret_cast<PHIDP_PREPARSED_DATA>(const_cast<char *>(m_preparsed.data()));

    ULONG x = 0, y = 0;
    if (HidP_GetUsageValue(HidP_Input, kPageDesktop, link, kUsageX, &x, pp, report, size) != HIDP_STATUS_SUCCESS
        || HidP_GetUsageValue(HidP_Input, kPageDesktop, link, kUsageY, &y, pp, report, size) != HIDP_STATUS_SUCCESS)
        return false;

    std::vector<USAGE> usages(std::max<ULONG>(m_maxUsages, 8));
    ULONG count = ULONG(usages.size());
    bool tip = false;
    bool confident = !m_hasConfidence;
    if (HidP_GetUsages(HidP_Input, kPageDigitizer, link, usages.data(), &count, pp, report, size) == HIDP_STATUS_SUCCESS) {
        for (ULONG i = 0; i < count; ++i) {
            tip |= usages[i] == kUsageTip;
            confident |= usages[i] == kUsageConfidence;
        }
    }

    ULONG id = 0;
    if (HidP_GetUsageValue(HidP_Input, kPageDigitizer, link, kUsageContactId, &id, pp, report, size) != HIDP_STATUS_SUCCESS)
        id = ULONG(index);

    out.id = int(id);
    out.tip = tip && confident;   // Windows ignores palms too
    out.x = int(x) * kScale;
    out.y = int(y) * kScale;
    return true;
}

bool TouchpadParser::feed(const unsigned char *data, unsigned long size, std::vector<TouchContact> &frame)
{
    char *report = reinterpret_cast<char *>(const_cast<unsigned char *>(data));
    const auto pp = reinterpret_cast<PHIDP_PREPARSED_DATA>(m_preparsed.data());

    if (!m_hasCount) {
        // No contact count: every report is a whole frame.
        frame.clear();
        bool any = false;
        for (size_t i = 0; i < m_fingers.size(); ++i) {
            TouchContact c;
            if (readContact(m_fingers[i], int(i), report, size, c)) {
                any = true;
                frame.push_back(c);
            }
        }
        return any;
    }

    ULONG count = 0;
    if (HidP_GetUsageValue(HidP_Input, kPageDigitizer, m_countLink, kUsageContactCount, &count, pp,
                           report, size) != HIDP_STATUS_SUCCESS)
        return false;   // some other report from the same device, not a touch frame

    if (count > 0) {
        // The first report of a frame says how many contacts the whole frame holds.
        m_pending.clear();
        m_remaining = int(count);
    } else if (m_remaining == 0) {
        frame.clear();
        return true;
    }

    const int n = std::min(int(m_fingers.size()), m_remaining);
    for (int i = 0; i < n; ++i) {
        TouchContact c;
        if (readContact(m_fingers[size_t(i)], int(m_pending.size()), report, size, c))
            m_pending.push_back(c);
    }
    m_remaining -= n;
    if (m_remaining > 0)
        return false;
    frame = std::move(m_pending);
    m_pending.clear();
    return true;
}
