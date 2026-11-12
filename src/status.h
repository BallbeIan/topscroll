#pragma once

#include <QByteArray>
#include <QString>

// What the daemon is doing, published in the runtime directory so the settings
// window and `topscroll status` can report it without talking to the process.
struct Status {
    // active, disabled, searching, denied, uinput or busy; the window adds "stopped"
    QString state;
    QString device;
    qint64 pid = 0;
    qint64 startedMs = 0;   // when the daemon started, ms since the epoch
    QString exe;            // the binary it was started from
    double widthMm = 0;
    double heightMm = 0;

    static QString defaultPath();
    static Status load(const QString &path);
    bool save(const QString &path) const;
    QByteArray toJson() const;
    static Status fromJson(const QByteArray &json);

    // The file outlives a crashed daemon, so check that its process still exists.
    bool alive() const;
    // True when the daemon is older than the binary on disk, which is what a
    // package upgrade leaves behind: the old process keeps running until it
    // is restarted. A daemon from before this field existed counts as old.
    bool needsRestart() const;
    bool operator==(const Status &) const = default;
};
