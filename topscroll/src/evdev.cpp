#include "evdev.h"

#include <fcntl.h>
#include <linux/uinput.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstring>

namespace {

template<size_t N>
void readBits(int fd, int type, std::bitset<N> &out)
{
    unsigned char buf[(N + 7) / 8] = {};
    if (ioctl(fd, EVIOCGBIT(type, sizeof buf), buf) < 0)
        return;
    for (size_t i = 0; i < N; ++i)
        if (buf[i / 8] & (1 << (i % 8)))
            out.set(i);
}

bool contains(const std::string &haystack, const char *needle)
{
    std::string lower = haystack;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return std::tolower(c); });
    return lower.find(needle) != std::string::npos;
}

}

bool DeviceCaps::isTouchpad() const
{
    // Our own virtual twin carries the same name plus a suffix; never grab it.
    if (name.size() >= 11 && name.compare(name.size() - 11, 11, "(topscroll)") == 0)
        return false;
    return abs.test(ABS_MT_POSITION_X) && key.test(BTN_TOOL_FINGER)
           && (contains(name, "touchpad") || contains(name, "trackpad"));
}

bool readCaps(int fd, DeviceCaps &c)
{
    char name[256] = {};
    if (ioctl(fd, EVIOCGNAME(sizeof name - 1), name) < 0)
        return false;
    c.name = name;
    ioctl(fd, EVIOCGID, &c.id);
    readBits(fd, 0, c.ev);
    readBits(fd, EV_KEY, c.key);
    readBits(fd, EV_ABS, c.abs);
    readBits(fd, EV_MSC, c.msc);

    unsigned char props[(INPUT_PROP_MAX + 8) / 8] = {};
    if (ioctl(fd, EVIOCGPROP(sizeof props), props) >= 0)
        for (size_t i = 0; i <= INPUT_PROP_MAX; ++i)
            if (props[i / 8] & (1 << (i % 8)))
                c.props.set(i);

    for (int code = 0; code <= ABS_MAX; ++code)
        if (c.abs.test(code))
            ioctl(fd, EVIOCGABS(code), &c.absinfo[code]);
    return true;
}

UInputDevice::~UInputDevice()
{
    if (m_fd >= 0) {
        ioctl(m_fd, UI_DEV_DESTROY);
        ::close(m_fd);
    }
}

bool UInputDevice::open()
{
    m_fd = ::open("/dev/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    if (m_fd < 0 && errno == ENOENT)
        m_fd = ::open("/dev/input/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    if (m_fd < 0)
        m_errno = errno;
    return m_fd >= 0;
}

bool UInputDevice::finish(const std::string &name, const input_id &id)
{
    uinput_setup us{};
    us.id = id;
    std::strncpy(us.name, name.c_str(), UINPUT_MAX_NAME_SIZE - 1);
    if (ioctl(m_fd, UI_DEV_SETUP, &us) < 0 || ioctl(m_fd, UI_DEV_CREATE) < 0) {
        m_errno = errno;
        ::close(m_fd);
        m_fd = -1;
        return false;
    }
    return true;
}

bool UInputDevice::createClone(const DeviceCaps &src, const std::string &name)
{
    if (!open())
        return false;
    for (int t = 1; t <= EV_MAX; ++t)   // EV_SYN is implicit
        if (src.ev.test(t))
            ioctl(m_fd, UI_SET_EVBIT, t);
    for (int c = 0; c <= KEY_MAX; ++c)
        if (src.key.test(c))
            ioctl(m_fd, UI_SET_KEYBIT, c);
    for (int c = 0; c <= MSC_MAX; ++c)
        if (src.msc.test(c))
            ioctl(m_fd, UI_SET_MSCBIT, c);
    for (int c = 0; c <= INPUT_PROP_MAX; ++c)
        if (src.props.test(c))
            ioctl(m_fd, UI_SET_PROPBIT, c);
    for (int c = 0; c <= ABS_MAX; ++c) {
        if (!src.abs.test(c))
            continue;
        ioctl(m_fd, UI_SET_ABSBIT, c);
        uinput_abs_setup as{};
        as.code = c;
        as.absinfo = src.absinfo[c];
        ioctl(m_fd, UI_ABS_SETUP, &as);
    }
    return finish(name, src.id);
}

bool UInputDevice::createWheel(const std::string &name)
{
    if (!open())
        return false;
    ioctl(m_fd, UI_SET_EVBIT, EV_REL);
    ioctl(m_fd, UI_SET_RELBIT, REL_WHEEL);
    ioctl(m_fd, UI_SET_RELBIT, REL_WHEEL_HI_RES);
    // A pointer device needs a button, or some stacks refuse to treat it as a mouse.
    ioctl(m_fd, UI_SET_EVBIT, EV_KEY);
    ioctl(m_fd, UI_SET_KEYBIT, BTN_LEFT);
    input_id id{};
    id.bustype = BUS_VIRTUAL;
    return finish(name, id);
}

void UInputDevice::write(int type, int code, int value)
{
    if (m_fd < 0)
        return;
    input_event ev{};
    ev.type = type;
    ev.code = code;
    ev.value = value;
    if (::write(m_fd, &ev, sizeof ev) < 0 && errno != EAGAIN)
        m_errno = errno;
}

void UInputDevice::syn()
{
    write(EV_SYN, SYN_REPORT, 0);
}
