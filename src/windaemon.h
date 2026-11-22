#pragma once

#include <QElapsedTimer>
#include <QFileSystemWatcher>
#include <QObject>
#include <QTimer>

#include <map>
#include <memory>
#include <vector>

#include "config.h"
#include "eventsink.h"
#include "scroller.h"
#include "status.h"
#include "wintouchpad.h"

// The Windows half of topscroll's background service. Windows has no way to
// grab a touchpad and republish it from user space, so the approach differs
// from Linux while the Scroller logic stays the same:
//
//  * The Precision Touchpad's contacts are read through Raw Input (they arrive
//    even when another window has focus) and translated into evdev-style
//    multitouch events for the Scroller.
//  * Windows keeps driving the cursor from the real touchpad. While a finger
//    that started in the strip is down, a low-level mouse hook swallows the
//    cursor movement and clicks it produces, so only the scroll happens.
//  * Wheel events are injected with SendInput, in the same 1/120-notch units
//    the Scroller already uses.
class Daemon : public QObject {
    Q_OBJECT
public:
    Daemon(QString configPath, QString statusPath, QObject *parent = nullptr);
    ~Daemon() override;

    void start();

    // Called from the window procedure and the mouse hook.
    void onRawInput(void *hRawInput);
    void onDeviceChange();
    bool shouldSwallowMouse(unsigned message) const;

private:
    void applyConfig();
    void armWatcher();
    void scan();
    void release();
    void onTick();
    void updateTick();
    void handleFrame(const std::vector<TouchContact> &contacts);
    void publish(const QString &state);
    ScrollOptions options() const;

    QString m_configPath;
    QString m_statusPath;
    Config m_config;
    Status m_status;

    QFileSystemWatcher m_watcher;
    QTimer m_reload;
    QTimer m_retry;
    QTimer m_tick;
    QElapsedTimer m_clock;
    QElapsedTimer m_sinceCapture;   // time since the last captured finger lifted

    void *m_hwnd = nullptr;          // hidden window that receives WM_INPUT
    void *m_hook = nullptr;          // WH_MOUSE_LL
    std::unique_ptr<TouchpadParser> m_pad;
    std::unique_ptr<EventSink> m_touchOut;
    std::unique_ptr<EventSink> m_wheelOut;
    std::unique_ptr<Scroller> m_scroller;

    // Windows contact ids mapped to the evdev slots the Scroller sees.
    struct Track { int slot; int x; int y; };
    std::map<int, Track> m_tracks;
    int m_nextTrackingId = 1;
    bool m_wasCaptured = false;
};
