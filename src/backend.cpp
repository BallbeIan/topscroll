#include "backend.h"

#ifdef Q_OS_WIN
#include <QCoreApplication>
#include <QDir>
#include <QSettings>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

Backend::Backend(const QString &configPath, const QString &statusPath, QObject *parent)
    : QObject(parent), m_configPath(configPath), m_statusPath(statusPath),
      m_config(Config::load(configPath))
{
    m_save.setSingleShot(true);
    m_save.setInterval(150);
    connect(&m_save, &QTimer::timeout, this, [this] { m_config.save(m_configPath); });

    m_poll.setInterval(700);
    connect(&m_poll, &QTimer::timeout, this, &Backend::refreshStatus);
    m_poll.start();
    refreshStatus();
}

Backend::~Backend()
{
    // Quitting right after a change must not lose it.
    if (m_save.isActive()) {
        m_save.stop();
        m_config.save(m_configPath);
    }
}

void Backend::change(const Config &next)
{
    Config c = next;
    c.clamp();
    if (c == m_config)
        return;
    m_config = c;
    emit configChanged();
    m_save.start();
}

void Backend::setEnabled(bool v) { Config c = m_config; c.enabled = v; change(c); }
void Backend::setStripMm(double v) { Config c = m_config; c.stripMm = v; change(c); }
void Backend::setMmPerNotch(double v) { Config c = m_config; c.mmPerNotch = v; change(c); }
void Backend::setInvert(bool v) { Config c = m_config; c.invert = v; change(c); }
void Backend::setCruise(bool v) { Config c = m_config; c.cruise = v; change(c); }
void Backend::setDeadMm(double v) { Config c = m_config; c.deadMm = v; change(c); }
void Backend::setCruiseStart(double v) { Config c = m_config; c.cruiseStart = v; change(c); }
void Backend::setCruiseMax(double v) { Config c = m_config; c.cruiseMax = v; change(c); }
void Backend::setInertia(bool v) { Config c = m_config; c.inertia = v; change(c); }
void Backend::setGlideSeconds(double v) { Config c = m_config; c.glideSeconds = v; change(c); }
void Backend::setSmooth(bool v) { Config c = m_config; c.smooth = v; change(c); }
void Backend::setSmoothSeconds(double v) { Config c = m_config; c.smoothSeconds = v; change(c); }
void Backend::setTouchpadStyle(bool v) { Config c = m_config; c.touchpadStyle = v; change(c); }

void Backend::resetDefaults()
{
    Config c;
    c.enabled = m_config.enabled;
    change(c);
}

void Backend::refreshStatus()
{
    const Status s = Status::load(m_statusPath);
    const QString state = s.alive() ? s.state : QStringLiteral("stopped");
    const Status shown = s.alive() ? s : Status{};
    const bool stale = s.needsRestart();
    if (state == m_state && shown == m_status && stale == m_needsRestart)
        return;
    m_needsRestart = stale;
    m_state = state;
    m_status = shown;
    emit statusChanged();
}

#ifdef Q_OS_WIN

// Windows has no user services; the daemon is the same executable started with
// `daemon`, launched at login from the per-user Run key.
static const char *kRunKey = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";

void Backend::startService()
{
    const QString exe = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    QSettings(kRunKey, QSettings::NativeFormat).setValue("topscroll", "\"" + exe + "\" daemon");
    QString err;
    if (!QProcess::startDetached(QCoreApplication::applicationFilePath(), {"daemon"}))
        err = tr("Couldn't start the background service.");
    if (err != m_serviceError) {
        m_serviceError = err;
        emit serviceErrorChanged();
    }
    QTimer::singleShot(800, this, &Backend::refreshStatus);
}

void Backend::restartService()
{
    if (m_status.pid > 0) {
        if (HANDLE h = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, DWORD(m_status.pid))) {
            TerminateProcess(h, 0);
            WaitForSingleObject(h, 2000);
            CloseHandle(h);
        }
    }
    startService();
}

#else

void Backend::startService()
{
    runSystemctl({"--user", "enable", "--now", "topscroll.service"});
}

void Backend::restartService()
{
    runSystemctl({"--user", "restart", "topscroll.service"});
}

#endif

void Backend::runSystemctl(const QStringList &args)
{
    auto *p = new QProcess(this);
    connect(p, &QProcess::finished, this, [this, p](int code, QProcess::ExitStatus) {
        const QString err = code == 0 ? QString() : QString::fromUtf8(p->readAllStandardError()).trimmed();
        if (err != m_serviceError) {
            m_serviceError = err;
            emit serviceErrorChanged();
        }
        p->deleteLater();
        QTimer::singleShot(500, this, &Backend::refreshStatus);
    });
    connect(p, &QProcess::errorOccurred, this, [this, p](QProcess::ProcessError) {
        m_serviceError = p->errorString();
        emit serviceErrorChanged();
        p->deleteLater();
    });
    p->start("systemctl", args);
}
