#include "status.h"

#include <QDir>
#include <QFile>
#include <QDateTime>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

QString Status::defaultPath()
{
    QString dir = qEnvironmentVariable("XDG_RUNTIME_DIR");
    if (dir.isEmpty())
        dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    return dir + "/topscroll/status.json";
}

QByteArray Status::toJson() const
{
    return QJsonDocument(QJsonObject{{"state", state},
                                     {"device", device},
                                     {"pid", double(pid)},
                                     {"started_ms", double(startedMs)},
                                     {"exe", exe},
                                     {"width_mm", widthMm},
                                     {"height_mm", heightMm}})
        .toJson(QJsonDocument::Compact);
}

Status Status::fromJson(const QByteArray &json)
{
    const QJsonObject o = QJsonDocument::fromJson(json).object();
    Status s;
    s.state = o["state"].toString();
    s.device = o["device"].toString();
    s.pid = qint64(o["pid"].toDouble());
    s.startedMs = qint64(o["started_ms"].toDouble());
    s.exe = o["exe"].toString();
    s.widthMm = o["width_mm"].toDouble();
    s.heightMm = o["height_mm"].toDouble();
    return s;
}

Status Status::load(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return fromJson(f.readAll());
}

bool Status::save(const QString &path) const
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        return false;
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    f.write(toJson() + '\n');
    return f.commit();
}

bool Status::alive() const
{
    return pid > 0 && QFileInfo::exists(QStringLiteral("/proc/%1").arg(pid));
}

bool Status::needsRestart() const
{
    if (!alive())
        return false;
    if (startedMs == 0)
        return true;
    const QFileInfo binary(exe);
    return binary.exists() && binary.lastModified().toMSecsSinceEpoch() > startedMs;
}
