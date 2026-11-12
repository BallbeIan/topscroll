#include "translator.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>

QString Translator::defaultLanguageFile()
{
    return QDir::homePath() + "/.config/omarchy/language";
}

Translator::Translator(const QString &languageFile, const QString &i18nDir, QObject *parent)
    : QTranslator(parent), m_file(languageFile), m_dir(i18nDir)
{
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(80);
    connect(&m_debounce, &QTimer::timeout, this, &Translator::reload);
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, &m_debounce, qOverload<>(&QTimer::start));
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, &m_debounce, qOverload<>(&QTimer::start));
    reload();
}

QString Translator::readLanguage() const
{
    QFile f(m_file);
    if (f.open(QIODevice::ReadOnly)) {
        QString code = QString::fromUtf8(f.readAll()).trimmed();
        if (!code.isEmpty())
            return code.replace('_', '-');
    }
    // Outside Omarchy there is no registry; follow the system locale instead.
    return QLocale::system().name().replace('_', '-');
}

void Translator::reload()
{
    const QString language = readLanguage();

    QStringList codes{language};
    if (const int dash = language.indexOf('-'); dash > 0)
        codes << language.left(dash);

    QList<QHash<QString, QString>> chain;
    for (const QString &code : std::as_const(codes)) {
        QFile f(m_dir + "/" + code + ".json");
        if (!f.open(QIODevice::ReadOnly))
            continue;
        QHash<QString, QString> dict;
        const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
        for (auto it = o.begin(); it != o.end(); ++it)
            dict.insert(it.key(), it.value().toString());
        chain << dict;
    }
    m_chain = chain;

    const bool changed = language != m_language;
    m_language = language;
    arm();
    if (changed)
        emit languageChanged();
}

void Translator::arm()
{
    // Omarchy rewrites the file when the language changes, so watch the file
    // and its directory and start over each time.
    const QStringList old = m_watcher.files() + m_watcher.directories();
    if (!old.isEmpty())
        m_watcher.removePaths(old);
    QStringList paths;
    if (QFileInfo::exists(m_file))
        paths << m_file;
    const QString dir = QFileInfo(m_file).absolutePath();
    if (QFileInfo(dir).isDir())
        paths << dir;
    if (!paths.isEmpty())
        m_watcher.addPaths(paths);
}

QString Translator::translate(const char *, const char *sourceText, const char *, int) const
{
    const QString key = QString::fromUtf8(sourceText);
    for (const auto &dict : m_chain) {
        const QString t = dict.value(key);
        if (!t.isEmpty())
            return t;
    }
    return {};   // empty means "not translated", so Qt shows the English source
}
