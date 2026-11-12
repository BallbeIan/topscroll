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
    double widthMm = 0;
    double heightMm = 0;

    static QString defaultPath();
    static Status load(const QString &path);
    bool save(const QString &path) const;
    QByteArray toJson() const;
    static Status fromJson(const QByteArray &json);

    // The file outlives a crashed daemon, so check that its process still exists.
    bool alive() const;
    bool operator==(const Status &) const = default;
};
