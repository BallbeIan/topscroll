#include "config.h"

#include <QDir>
#include <QFileInfo>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>

#include <algorithm>

QString Config::defaultPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
           + "/topscroll/topscroll.conf";
}

void Config::clamp()
{
    stripMm = std::clamp(stripMm, kMinStripMm, kMaxStripMm);
    mmPerNotch = std::clamp(mmPerNotch, kMinMmPerNotch, kMaxMmPerNotch);
}

Config Config::load(const QString &path)
{
    Config c;
    if (QFileInfo::exists(path)) {
        QSettings s(path, QSettings::IniFormat);
        c.enabled = s.value("enabled", c.enabled).toBool();
        c.stripMm = s.value("strip_mm", c.stripMm).toDouble();
        c.mmPerNotch = s.value("mm_per_notch", c.mmPerNotch).toDouble();
        c.invert = s.value("invert", c.invert).toBool();
    }
    c.clamp();
    return c;
}

bool Config::save(const QString &path) const
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        return false;
    // Written beside the target and renamed, so the daemon never reads a half-written file.
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    const QString text = QStringLiteral("[General]\nenabled=%1\nstrip_mm=%2\nmm_per_notch=%3\ninvert=%4\n")
                             .arg(enabled ? "true" : "false")
                             .arg(stripMm, 0, 'f', 1)
                             .arg(mmPerNotch, 0, 'f', 1)
                             .arg(invert ? "true" : "false");
    f.write(text.toUtf8());
    return f.commit();
}
