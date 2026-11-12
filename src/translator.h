#pragma once

#include <QFileSystemWatcher>
#include <QHash>
#include <QString>
#include <QTimer>
#include <QTranslator>

// Translates qsTr()/tr() strings from JSON files (English text -> translation)
// in the language chosen in Omarchy's Setup > Language, live. A regional code
// such as pt-BR falls back to pt and then to the English source, per string.
class Translator : public QTranslator {
    Q_OBJECT
public:
    explicit Translator(const QString &languageFile = defaultLanguageFile(),
                        const QString &i18nDir = QStringLiteral(":/i18n"), QObject *parent = nullptr);

    static QString defaultLanguageFile();

    QString language() const { return m_language; }
    QString translate(const char *context, const char *sourceText, const char *disambiguation,
                      int n) const override;
    bool isEmpty() const override { return false; }

signals:
    void languageChanged();

private:
    void reload();
    void arm();
    QString readLanguage() const;

    QString m_file;
    QString m_dir;
    QString m_language;
    QList<QHash<QString, QString>> m_chain;   // most specific language first
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
};
