#include "theme.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

QString Theme::defaultColorsFile()
{
    return QDir::homePath() + "/.local/state/omarchy/current/theme/colors.toml";
}

QString Theme::parseAccent(const QString &toml)
{
    static const QRegularExpression re(R"(^\s*accent\s*=\s*["']?(#[0-9a-fA-F]{6})["']?)",
                                       QRegularExpression::MultilineOption);
    const auto m = re.match(toml);
    return m.hasMatch() ? m.captured(1) : QString::fromLatin1(kFallbackAccent);
}

Theme::Theme(const QString &colorsFile, QObject *parent) : QObject(parent), m_file(colorsFile)
{
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(80);
    connect(&m_debounce, &QTimer::timeout, this, [this] {
        reload();
        refreshRadius();
    });
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, &m_debounce, qOverload<>(&QTimer::start));
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, &m_debounce, qOverload<>(&QTimer::start));
    reload();
}

void Theme::arm()
{
    // Theme switches replace the file, its directory or a symlink above it, so
    // watch every level that exists and start over each time.
    const QStringList old = m_watcher.files() + m_watcher.directories();
    if (!old.isEmpty())
        m_watcher.removePaths(old);
    QDir dir = QFileInfo(m_file).dir();
    QStringList paths;
    if (QFileInfo::exists(m_file))
        paths << m_file;
    for (int i = 0; i < 2; ++i) {
        if (dir.exists())
            paths << dir.absolutePath();
        if (!dir.cdUp())
            break;
    }
    if (!paths.isEmpty())
        m_watcher.addPaths(paths);
}

void Theme::reload()
{
    QString accent = QString::fromLatin1(kFallbackAccent);
    QFile f(m_file);
    if (f.open(QIODevice::ReadOnly))
        accent = parseAccent(QString::fromUtf8(f.readAll()));
    if (accent != m_accent) {
        m_accent = accent;
        emit accentChanged();
    }
    arm();
}

void Theme::refreshRadius()
{
    if (QStandardPaths::findExecutable("hyprctl").isEmpty())
        return;
    auto *p = new QProcess(this);
    connect(p, &QProcess::finished, this, [this, p](int code, QProcess::ExitStatus) {
        int radius = 0;
        if (code == 0)
            radius = QJsonDocument::fromJson(p->readAllStandardOutput()).object()["int"].toInt();
        radius = qMax(0, radius);
        if (radius != m_radius) {
            m_radius = radius;
            emit radiusChanged();
        }
        p->deleteLater();
    });
    connect(p, &QProcess::errorOccurred, p, &QObject::deleteLater);
    p->start("hyprctl", {"getoption", "decoration:rounding", "-j"});
}
