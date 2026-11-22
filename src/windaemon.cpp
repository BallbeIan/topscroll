#include "windaemon.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <algorithm>
#include <set>

#include "inputcodes.h"

namespace {

Daemon *g_daemon = nullptr;

// Windows still moves the cursor from the real touchpad, so the "virtual
// touchpad" of the Linux version has nothing to do here.
class NullSink : public EventSink {
public:
    void write(int, int, int) override {}
    void syn() override {}
};

// Wheel events through SendInput. WHEEL_DELTA is 120 per notch, the same unit
// as REL_WHEEL_HI_RES, and positive means up on both, so values pass straight
// through. REL_WHEEL (whole notches) would count twice and is ignored.
class WheelSink : public EventSink {
public:
    void write(int type, int code, int value) override
    {
        if (type == EV_REL && code == REL_WHEEL_HI_RES)
            m_pending += value;
    }
    void syn() override
    {
        if (!m_pending)
            return;
        INPUT in{};
        in.type = INPUT_MOUSE;
        in.mi.dwFlags = MOUSEEVENTF_WHEEL;
        in.mi.mouseData = DWORD(m_pending);
        SendInput(1, &in, sizeof in);
        m_pending = 0;
    }

private:
    int m_pending = 0;
};

// A tap that started in the strip is a scroll gesture, not a click. Windows
// turns the tap into a click just after the finger lifts, so button presses
// are held back for this long after a strip touch ends.
constexpr qint64 kTapGraceMs = 250;

LRESULT CALLBACK windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (g_daemon) {
        if (msg == WM_INPUT)
            g_daemon->onRawInput(reinterpret_cast<void *>(lParam));
        else if (msg == WM_INPUT_DEVICE_CHANGE)
            g_daemon->onDeviceChange();
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// Which button a message belongs to, and whether it presses or releases it.
int buttonOf(WPARAM msg, bool &down)
{
    switch (msg) {
    case WM_LBUTTONDOWN: down = true; return 0;
    case WM_LBUTTONUP: down = false; return 0;
    case WM_RBUTTONDOWN: down = true; return 1;
    case WM_RBUTTONUP: down = false; return 1;
    case WM_MBUTTONDOWN: down = true; return 2;
    case WM_MBUTTONUP: down = false; return 2;
    default: return -1;
    }
}

unsigned g_swallowedDown = 0;   // buttons whose press was swallowed, so their release is too

LRESULT CALLBACK mouseHook(int code, WPARAM wParam, LPARAM lParam)
{
    if (code == HC_ACTION && g_daemon) {
        const auto *info = reinterpret_cast<const MSLLHOOKSTRUCT *>(lParam);
        if (!(info->flags & LLMHF_INJECTED)) {
            bool down = false;
            const int button = buttonOf(wParam, down);
            if (button >= 0 && !down && (g_swallowedDown & (1u << button))) {
                g_swallowedDown &= ~(1u << button);
                return 1;
            }
            if (g_daemon->shouldSwallowMouse(unsigned(wParam))) {
                if (button >= 0 && down)
                    g_swallowedDown |= 1u << button;
                if (button < 0 || down)
                    return 1;
            }
        }
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

}

Daemon::Daemon(QString configPath, QString statusPath, QObject *parent)
    : QObject(parent), m_configPath(std::move(configPath)), m_statusPath(std::move(statusPath))
{
    g_daemon = this;

    m_reload.setSingleShot(true);
    m_reload.setInterval(100);
    connect(&m_reload, &QTimer::timeout, this, &Daemon::applyConfig);
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, &m_reload, qOverload<>(&QTimer::start));
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, &m_reload, qOverload<>(&QTimer::start));

    m_tick.setInterval(10);
    m_tick.setTimerType(Qt::PreciseTimer);
    connect(&m_tick, &QTimer::timeout, this, &Daemon::onTick);

    m_retry.setInterval(3000);
    connect(&m_retry, &QTimer::timeout, this, &Daemon::scan);

    m_status.pid = QCoreApplication::applicationPid();
    m_status.startedMs = QDateTime::currentMSecsSinceEpoch();
    m_status.exe = QCoreApplication::applicationFilePath();

    // A hidden window to receive Raw Input. It must be a real (never shown)
    // top-level window: RIDEV_INPUTSINK needs a target window.
    const HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof wc;
    wc.lpfnWndProc = windowProc;
    wc.hInstance = instance;
    wc.lpszClassName = L"topscrollDaemon";
    RegisterClassExW(&wc);
    m_hwnd = CreateWindowExW(0, wc.lpszClassName, L"topscroll", WS_OVERLAPPED, 0, 0, 0, 0,
                             nullptr, nullptr, instance, nullptr);
}

Daemon::~Daemon()
{
    release();
    if (m_hwnd)
        DestroyWindow(HWND(m_hwnd));
    g_daemon = nullptr;
    QFile::remove(m_statusPath);
}

void Daemon::start()
{
    QDir().mkpath(QFileInfo(m_configPath).absolutePath());
    applyConfig();
}

void Daemon::armWatcher()
{
    const QStringList old = m_watcher.files() + m_watcher.directories();
    if (!old.isEmpty())
        m_watcher.removePaths(old);
    QStringList paths{QFileInfo(m_configPath).absolutePath()};
    if (QFileInfo::exists(m_configPath))
        paths << m_configPath;
    m_watcher.addPaths(paths);
}

ScrollOptions Daemon::options() const
{
    return {m_config.stripMm, m_config.mmPerNotch, m_config.invert, m_config.cruise,
            m_config.deadMm, m_config.cruiseStart, m_config.cruiseMax,
            m_config.inertia, m_config.glideSeconds,
            m_config.smooth, m_config.smoothSeconds, m_config.touchpadStyle};
}

void Daemon::applyConfig()
{
    armWatcher();
    m_config = Config::load(m_configPath);

    if (!m_config.enabled) {
        m_retry.stop();
        release();
        publish("disabled");
        return;
    }
    if (m_scroller) {
        m_scroller->setOptions(options());
        updateTick();
        return;
    }
    scan();
}

void Daemon::scan()
{
    m_retry.stop();
    release();

    UINT count = 0;
    GetRawInputDeviceList(nullptr, &count, sizeof(RAWINPUTDEVICELIST));
    std::vector<RAWINPUTDEVICELIST> list(count);
    if (count)
        count = GetRawInputDeviceList(list.data(), &count, sizeof(RAWINPUTDEVICELIST));
    if (count == UINT(-1))
        count = 0;

    for (UINT i = 0; i < count && !m_pad; ++i) {
        if (list[i].dwType == RIM_TYPEHID)
            m_pad = TouchpadParser::open(list[i].hDevice);
    }
    if (!m_pad || !m_hwnd) {
        // No Precision Touchpad (yet): keep looking, it may be plugged in later.
        m_pad.reset();
        publish("searching");
        m_retry.start();
        return;
    }

    RAWINPUTDEVICE rid{};
    rid.usUsagePage = 0x0D;
    rid.usUsage = 0x05;
    rid.dwFlags = RIDEV_INPUTSINK | RIDEV_DEVNOTIFY;
    rid.hwndTarget = HWND(m_hwnd);
    if (!RegisterRawInputDevices(&rid, 1, sizeof rid)) {
        m_pad.reset();
        publish("uinput");
        m_retry.start();
        return;
    }
    m_hook = SetWindowsHookExW(WH_MOUSE_LL, mouseHook, GetModuleHandleW(nullptr), 0);
    if (!m_hook) {
        release();
        publish("uinput");
        m_retry.start();
        return;
    }

    m_touchOut = std::make_unique<NullSink>();
    m_wheelOut = std::make_unique<WheelSink>();
    m_scroller = std::make_unique<Scroller>(*m_touchOut, *m_wheelOut, m_pad->x(), m_pad->y(), options());
    publish("active");
}

void Daemon::release()
{
    m_tick.stop();
    if (m_hook) {
        UnhookWindowsHookEx(HHOOK(m_hook));
        m_hook = nullptr;
    }
    g_swallowedDown = 0;
    if (m_pad) {
        RAWINPUTDEVICE rid{};
        rid.usUsagePage = 0x0D;
        rid.usUsage = 0x05;
        rid.dwFlags = RIDEV_REMOVE;
        rid.hwndTarget = nullptr;
        RegisterRawInputDevices(&rid, 1, sizeof rid);
    }
    m_scroller.reset();
    m_touchOut.reset();
    m_wheelOut.reset();
    m_pad.reset();
    m_tracks.clear();
    m_wasCaptured = false;
}

void Daemon::onDeviceChange()
{
    // A device came or went. If ours vanished, or we have none, look again.
    if (!m_config.enabled)
        return;
    if (m_pad) {
        RID_DEVICE_INFO info{};
        info.cbSize = sizeof info;
        UINT size = sizeof info;
        if (GetRawInputDeviceInfoW(HANDLE(m_pad->device()), RIDI_DEVICEINFO, &info, &size) != UINT(-1))
            return;   // still here
    }
    QTimer::singleShot(500, this, &Daemon::scan);
}

void Daemon::onRawInput(void *handle)
{
    if (!m_scroller || !m_pad)
        return;
    UINT size = 0;
    GetRawInputData(HRAWINPUT(handle), RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER));
    if (!size)
        return;
    std::vector<BYTE> buf(size);
    if (GetRawInputData(HRAWINPUT(handle), RID_INPUT, buf.data(), &size, sizeof(RAWINPUTHEADER)) == UINT(-1))
        return;
    const auto *raw = reinterpret_cast<const RAWINPUT *>(buf.data());
    if (raw->header.dwType != RIM_TYPEHID || raw->header.hDevice != HANDLE(m_pad->device()))
        return;

    const DWORD reportSize = raw->data.hid.dwSizeHid;
    std::vector<TouchContact> frame;
    for (DWORD i = 0; i < raw->data.hid.dwCount; ++i) {
        if (m_pad->feed(raw->data.hid.bRawData + i * reportSize, reportSize, frame))
            handleFrame(frame);
    }
}

void Daemon::handleFrame(const std::vector<TouchContact> &contacts)
{
    // Translate the frame into the evdev multitouch protocol the Scroller speaks.
    std::set<int> down;
    for (const TouchContact &c : contacts) {
        if (!c.tip || down.count(c.id))
            continue;
        down.insert(c.id);
        auto it = m_tracks.find(c.id);
        if (it == m_tracks.end()) {
            int slot = 0;
            while (std::any_of(m_tracks.begin(), m_tracks.end(),
                               [slot](const auto &t) { return t.second.slot == slot; }))
                ++slot;
            m_tracks[c.id] = {slot, c.x, c.y};
            m_scroller->handle(EV_ABS, ABS_MT_SLOT, slot);
            m_scroller->handle(EV_ABS, ABS_MT_TRACKING_ID, m_nextTrackingId++);
            m_scroller->handle(EV_ABS, ABS_MT_POSITION_X, c.x);
            m_scroller->handle(EV_ABS, ABS_MT_POSITION_Y, c.y);
            if (m_nextTrackingId > 0xFFFF)
                m_nextTrackingId = 1;
        } else {
            Track &t = it->second;
            if (c.x != t.x || c.y != t.y)
                m_scroller->handle(EV_ABS, ABS_MT_SLOT, t.slot);
            if (c.x != t.x)
                m_scroller->handle(EV_ABS, ABS_MT_POSITION_X, c.x);
            if (c.y != t.y)
                m_scroller->handle(EV_ABS, ABS_MT_POSITION_Y, c.y);
            t.x = c.x;
            t.y = c.y;
        }
    }
    for (auto it = m_tracks.begin(); it != m_tracks.end();) {
        if (down.count(it->first)) {
            ++it;
            continue;
        }
        m_scroller->handle(EV_ABS, ABS_MT_SLOT, it->second.slot);
        m_scroller->handle(EV_ABS, ABS_MT_TRACKING_ID, -1);
        it = m_tracks.erase(it);
    }
    m_scroller->handle(EV_SYN, SYN_REPORT, 0);

    const bool captured = m_scroller->hasCaptured();
    if (m_wasCaptured && !captured)
        m_sinceCapture.start();
    m_wasCaptured = captured;
    updateTick();
}

bool Daemon::shouldSwallowMouse(unsigned message) const
{
    if (!m_scroller)
        return false;
    const bool captured = m_scroller->hasCaptured();
    if (message == WM_MOUSEMOVE)
        return captured;
    if (message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN || message == WM_MBUTTONDOWN)
        return captured || (m_sinceCapture.isValid() && m_sinceCapture.elapsed() < kTapGraceMs);
    return false;
}

void Daemon::updateTick()
{
    if (m_scroller && m_scroller->needsTick()) {
        if (!m_tick.isActive()) {
            m_clock.restart();
            m_tick.start();
        }
    } else {
        m_tick.stop();
    }
}

void Daemon::onTick()
{
    if (!m_scroller)
        return;
    m_scroller->tick(std::min(0.1, m_clock.restart() / 1000.0));
    updateTick();
}

void Daemon::publish(const QString &state)
{
    Status s = m_status;
    s.state = state;
    s.device = (state == "active" && m_pad) ? m_pad->name() : QString();
    s.widthMm = s.heightMm = 0;
    if (state == "active" && m_pad) {
        s.widthMm = Scroller::extentMm(m_pad->x());
        s.heightMm = Scroller::extentMm(m_pad->y());
    }
    if (s == m_status && QFileInfo::exists(m_statusPath))
        return;
    m_status = s;
    m_status.save(m_statusPath);
}
