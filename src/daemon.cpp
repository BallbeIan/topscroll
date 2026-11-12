#include "daemon.h"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <algorithm>
#include <cerrno>

Daemon::Daemon(QString configPath, QString statusPath, QObject *parent)
    : QObject(parent), m_configPath(std::move(configPath)), m_statusPath(std::move(statusPath))
{
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
}

Daemon::~Daemon()
{
    release();
    QFile::remove(m_statusPath);
}

void Daemon::start()
{
    QDir().mkpath(QFileInfo(m_configPath).absolutePath());
    applyConfig();
}

void Daemon::armWatcher()
{
    // Saving replaces the file, which drops it from the watcher; watch the
    // directory too and re-add the file after every change.
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
    return {m_config.stripMm, m_config.mmPerNotch, m_config.invert, m_config.cruise};
}

void Daemon::applyConfig()
{
    armWatcher();
    m_config = Config::load(m_configPath);

    if (!m_config.enabled) {
        // Letting go of the touchpad entirely means it behaves as if we weren't here.
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
    QString failure = "searching";
    bool denied = false;

    const QStringList nodes = QDir("/dev/input").entryList({"event*"}, QDir::System, QDir::Name);
    for (const QString &node : nodes) {
        const QByteArray path = ("/dev/input/" + node).toLocal8Bit();
        const int fd = ::open(path.constData(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) {
            denied |= (errno == EACCES || errno == EPERM);
            continue;
        }
        DeviceCaps caps;
        if (!readCaps(fd, caps) || !caps.isTouchpad()) {
            ::close(fd);
            continue;
        }
        const QString state = attach(fd, caps);
        if (state.isEmpty())
            return;
        failure = state;   // keep looking: another touchpad may be free
    }
    if (failure == "searching" && denied)
        failure = "denied";

    publish(failure);
    m_retry.start();
}

QString Daemon::attach(int fd, const DeviceCaps &caps)
{
    auto touch = std::make_unique<UInputDevice>();
    auto wheel = std::make_unique<UInputDevice>();
    if (!touch->createClone(caps, caps.name + " (topscroll)") || !wheel->createWheel("topscroll wheel")) {
        ::close(fd);
        return "uinput";
    }
    if (ioctl(fd, EVIOCGRAB, 1) < 0) {
        ::close(fd);
        return "busy";
    }

    m_fd = fd;
    m_touchOut = std::move(touch);
    m_wheelOut = std::move(wheel);
    const auto axis = [&](int code) {
        const input_absinfo &a = caps.absinfo[code];
        return AxisInfo{a.minimum, a.maximum, a.resolution};
    };
    m_scroller = std::make_unique<Scroller>(*m_touchOut, *m_wheelOut, axis(ABS_MT_POSITION_X),
                                            axis(ABS_MT_POSITION_Y), options());
    m_notifier = std::make_unique<QSocketNotifier>(m_fd, QSocketNotifier::Read);
    connect(m_notifier.get(), &QSocketNotifier::activated, this, &Daemon::onReadable);
    publish("active", &caps);
    return {};
}

void Daemon::release()
{
    if (m_notifier) {
        // release() can run from this notifier's own slot, so it must not be deleted in place.
        m_notifier->setEnabled(false);
        m_notifier.release()->deleteLater();
    }
    m_tick.stop();
    m_scroller.reset();
    if (m_fd >= 0) {
        ioctl(m_fd, EVIOCGRAB, 0);
        ::close(m_fd);
        m_fd = -1;
    }
    // Destroying the virtual devices lifts any touch still held down.
    m_touchOut.reset();
    m_wheelOut.reset();
}

void Daemon::onReadable()
{
    input_event events[64];
    for (;;) {
        const ssize_t n = ::read(m_fd, events, sizeof events);
        if (n < 0 && errno == EAGAIN)
            return;
        if (n <= 0) {
            // The touchpad went away (unplugged, or the machine resumed): look for it again.
            release();
            publish("searching");
            m_retry.start();
            return;
        }
        for (size_t i = 0; i < size_t(n) / sizeof(input_event); ++i)
            m_scroller->handle(events[i].type, events[i].code, events[i].value);
        updateTick();
    }
}

void Daemon::updateTick()
{
    // A resting finger sends no events, so cruising needs a clock of its own.
    // It runs only while a finger is far enough from where it landed to scroll.
    if (m_scroller && m_scroller->cruising()) {
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
    // Cap the step so a stalled event loop can't fire one huge scroll.
    m_scroller->tick(std::min(0.1, m_clock.restart() / 1000.0));
    updateTick();
}

void Daemon::publish(const QString &state, const DeviceCaps *caps)
{
    Status s = m_status;
    s.state = state;
    s.device = caps ? QString::fromStdString(caps->name) : QString();
    s.widthMm = s.heightMm = 0;
    if (caps) {
        const auto extent = [&](int code) {
            const input_absinfo &a = caps->absinfo[code];
            return Scroller::extentMm({a.minimum, a.maximum, a.resolution});
        };
        s.widthMm = extent(ABS_MT_POSITION_X);
        s.heightMm = extent(ABS_MT_POSITION_Y);
    }
    if (s == m_status && QFileInfo::exists(m_statusPath))
        return;
    m_status = s;
    m_status.save(m_statusPath);
}
