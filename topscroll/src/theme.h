#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QString>
#include <QTimer>

// Follows the Omarchy theme: the accent colour from the current theme's
// colors.toml and the window rounding Hyprland is configured with. A theme
// switch swaps files and symlinks, so the watcher is re-armed on every change.
class Theme : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString accent READ accent NOTIFY accentChanged)
    Q_PROPERTY(int radius READ radius NOTIFY radiusChanged)

public:
    static constexpr const char *kFallbackAccent = "#FFD60A";

    explicit Theme(const QString &colorsFile = defaultColorsFile(), QObject *parent = nullptr);

    static QString defaultColorsFile();
    // Extracts `accent = "#rrggbb"` from a colors.toml, or returns the fallback.
    static QString parseAccent(const QString &toml);

    QString accent() const { return m_accent; }
    int radius() const { return m_radius; }

    // Asks hyprctl for the effective `decoration:rounding`. Not part of the
    // constructor so tests never spawn a process.
    void refreshRadius();

signals:
    void accentChanged();
    void radiusChanged();

private:
    void reload();
    void arm();

    QString m_file;
    QString m_accent = kFallbackAccent;
    int m_radius = 0;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
};
