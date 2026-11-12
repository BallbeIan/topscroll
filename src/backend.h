#pragma once

#include <QObject>
#include <QProcess>
#include <QTimer>

#include "config.h"
#include "status.h"

// What the settings window shows and changes. Settings are written to the
// config file, which the daemon watches; the daemon's state comes back through
// the status file.
class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY configChanged)
    Q_PROPERTY(double stripMm READ stripMm WRITE setStripMm NOTIFY configChanged)
    Q_PROPERTY(double mmPerNotch READ mmPerNotch WRITE setMmPerNotch NOTIFY configChanged)
    Q_PROPERTY(bool invert READ invert WRITE setInvert NOTIFY configChanged)
    Q_PROPERTY(bool cruise READ cruise WRITE setCruise NOTIFY configChanged)
    Q_PROPERTY(double deadMm READ deadMm WRITE setDeadMm NOTIFY configChanged)
    Q_PROPERTY(double cruiseStart READ cruiseStart WRITE setCruiseStart NOTIFY configChanged)
    Q_PROPERTY(double cruiseMax READ cruiseMax WRITE setCruiseMax NOTIFY configChanged)
    Q_PROPERTY(bool inertia READ inertia WRITE setInertia NOTIFY configChanged)
    Q_PROPERTY(double glideSeconds READ glideSeconds WRITE setGlideSeconds NOTIFY configChanged)

    // active, disabled, searching, denied, uinput, busy, or stopped (no daemon process)
    Q_PROPERTY(QString state READ state NOTIFY statusChanged)
    Q_PROPERTY(QString device READ device NOTIFY statusChanged)
    Q_PROPERTY(double padWidthMm READ padWidthMm NOTIFY statusChanged)
    Q_PROPERTY(double padHeightMm READ padHeightMm NOTIFY statusChanged)
    Q_PROPERTY(bool needsRestart READ needsRestart NOTIFY statusChanged)
    Q_PROPERTY(QString serviceError READ serviceError NOTIFY serviceErrorChanged)

public:
    explicit Backend(const QString &configPath = Config::defaultPath(),
                     const QString &statusPath = Status::defaultPath(), QObject *parent = nullptr);
    ~Backend() override;

    bool enabled() const { return m_config.enabled; }
    double stripMm() const { return m_config.stripMm; }
    double mmPerNotch() const { return m_config.mmPerNotch; }
    bool invert() const { return m_config.invert; }
    bool cruise() const { return m_config.cruise; }
    double deadMm() const { return m_config.deadMm; }
    double cruiseStart() const { return m_config.cruiseStart; }
    double cruiseMax() const { return m_config.cruiseMax; }
    bool inertia() const { return m_config.inertia; }
    double glideSeconds() const { return m_config.glideSeconds; }
    void setEnabled(bool v);
    void setStripMm(double v);
    void setMmPerNotch(double v);
    void setInvert(bool v);
    void setCruise(bool v);
    void setDeadMm(double v);
    void setCruiseStart(double v);
    void setCruiseMax(double v);
    void setInertia(bool v);
    void setGlideSeconds(double v);

    QString state() const { return m_state; }
    QString device() const { return m_status.device; }
    double padWidthMm() const { return m_status.widthMm; }
    double padHeightMm() const { return m_status.heightMm; }
    bool needsRestart() const { return m_needsRestart; }
    QString serviceError() const { return m_serviceError; }

    // Enables and starts the systemd user service that runs the daemon.
    Q_INVOKABLE void startService();
    // Restarts it, for after an upgrade left the old daemon running.
    Q_INVOKABLE void restartService();
    // Puts every setting back to its default, except whether scrolling is on.
    Q_INVOKABLE void resetDefaults();

signals:
    void configChanged();
    void statusChanged();
    void serviceErrorChanged();

private:
    void change(const Config &next);
    void refreshStatus();
    void runSystemctl(const QStringList &args);

    QString m_configPath;
    QString m_statusPath;
    Config m_config;
    Status m_status;
    QString m_state = "stopped";
    bool m_needsRestart = false;
    QString m_serviceError;
    QTimer m_save;   // coalesces slider drags into one write
    QTimer m_poll;
};
