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
    double deadMm = 1.5;        // cruise: ignored movement around where the finger landed
    double cruiseStart = 2.0;   // cruise: notches per second when it starts
    double cruiseMax = 60.0;    // cruise: top speed, in notches per second
    bool inertia = true;        // keep gliding after the finger lifts
    double glideSeconds = 1.0;  // roughly how long the glide lasts

    static constexpr double kMinStripMm = 4, kMaxStripMm = 30;
    static constexpr double kMinMmPerNotch = 2, kMaxMmPerNotch = 12;
    static constexpr double kMinDeadMm = 0.5, kMaxDeadMm = 8;
    static constexpr double kMinCruiseStart = 0.5, kMaxCruiseStart = 10;
    static constexpr double kMinCruiseMax = 10, kMaxCruiseMax = 120;
    static constexpr double kMinGlideSeconds = 0.2, kMaxGlideSeconds = 3;

    static QString defaultPath();
    static Config load(const QString &path);
    bool save(const QString &path) const;
    void clamp();
    bool operator==(const Config &) const = default;
};
