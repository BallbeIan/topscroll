#pragma once

#include <QString>

// The user's settings, shared by the settings window (which writes them) and
// the background daemon (which watches the file and applies them live).
struct Config {
    bool enabled = true;
    double stripMm = 12.0;
    double mmPerNotch = 5.0;
    bool invert = false;
    bool cruise = false;

    static constexpr double kMinStripMm = 4, kMaxStripMm = 30;
    static constexpr double kMinMmPerNotch = 2, kMaxMmPerNotch = 12;

    static QString defaultPath();
    static Config load(const QString &path);
    bool save(const QString &path) const;
    void clamp();
    bool operator==(const Config &) const = default;
};
