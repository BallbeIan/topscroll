#pragma once

#include <QFileSystemWatcher>
#include <QElapsedTimer>
#include <QObject>
#include <QSocketNotifier>
#include <QTimer>

#include <memory>

#include "config.h"
#include "evdev.h"
#include "scroller.h"
#include "status.h"

// The background half of topscroll: grabs the touchpad, republishes it through
// a virtual one and turns top-edge slides into wheel events. It follows the
// config file live, looks for a touchpad again when one appears, and writes
// what it is doing to the status file.
class Daemon : public QObject {
    Q_OBJECT
public:
    Daemon(QString configPath, QString statusPath, QObject *parent = nullptr);
    ~Daemon() override;

    void start();

private:
    void applyConfig();
    void armWatcher();
    void scan();
    QString attach(int fd, const DeviceCaps &caps);   // empty on success, else the status state
    void release();
    void onReadable();
    void onTick();
    void updateTick();
    void publish(const QString &state, const DeviceCaps *caps = nullptr);
    ScrollOptions options() const;

    QString m_configPath;
    QString m_statusPath;
    Config m_config;
    Status m_status;

    QFileSystemWatcher m_watcher;
    QTimer m_reload;   // debounces bursts of file events into one reload
    QTimer m_retry;    // looks for a touchpad again while none is attached
    QTimer m_tick;     // advances cruising and glides; runs only while needed
    QElapsedTimer m_clock;

    int m_fd = -1;
    std::unique_ptr<QSocketNotifier> m_notifier;
    std::unique_ptr<UInputDevice> m_touchOut;
    std::unique_ptr<UInputDevice> m_wheelOut;
    std::unique_ptr<Scroller> m_scroller;
};
