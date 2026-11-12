#include <QTemporaryDir>
#include <QtTest>

#include <linux/input.h>

#include <vector>

#include "../src/config.h"
#include "../src/scroller.h"
#include "../src/status.h"
#include "../src/theme.h"
#include "../src/translator.h"

namespace {

struct Event {
    int type, code, value;
    bool operator==(const Event &) const = default;
};

struct RecordingSink : EventSink {
    std::vector<Event> events;
    void write(int type, int code, int value) override { events.push_back({type, code, value}); }
    void syn() override { write(EV_SYN, SYN_REPORT, 0); }

    int count(int type, int code) const
    {
        int n = 0;
        for (const auto &e : events)
            n += e.type == type && e.code == code;
        return n;
    }
    int sum(int type, int code) const
    {
        int n = 0;
        for (const auto &e : events)
            if (e.type == type && e.code == code)
                n += e.value;
        return n;
    }
};

void writeFile(const QString &path, const QByteArray &data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}

}

class TopscrollTests : public QObject {
    Q_OBJECT

    // A 100 x 70 mm pad at 10 units per mm: the 12 mm strip is the top 120 units.
    RecordingSink touch, wheel;
    std::unique_ptr<Scroller> s;

    void frame() { s->handle(EV_SYN, SYN_REPORT, 0); }
    void down(int slot, int id, int x, int y)
    {
        s->handle(EV_ABS, ABS_MT_SLOT, slot);
        s->handle(EV_ABS, ABS_MT_TRACKING_ID, id);
        s->handle(EV_ABS, ABS_MT_POSITION_X, x);
        s->handle(EV_ABS, ABS_MT_POSITION_Y, y);
        frame();
    }
    void moveTo(int slot, int x, int y)
    {
        s->handle(EV_ABS, ABS_MT_SLOT, slot);
        s->handle(EV_ABS, ABS_MT_POSITION_X, x);
        s->handle(EV_ABS, ABS_MT_POSITION_Y, y);
        frame();
    }
    void up(int slot)
    {
        s->handle(EV_ABS, ABS_MT_SLOT, slot);
        s->handle(EV_ABS, ABS_MT_TRACKING_ID, -1);
        frame();
    }
    void makeScroller(ScrollOptions o = {})
    {
        touch.events.clear();
        wheel.events.clear();
        s = std::make_unique<Scroller>(touch, wheel, AxisInfo{0, 1000, 10}, AxisInfo{0, 700, 10}, o);
    }

private slots:
    void init() { makeScroller(); }

    void touchBelowTheStripPassesThrough()
    {
        down(0, 1, 500, 400);
        moveTo(0, 520, 410);
        up(0);
        QCOMPARE(wheel.events.size(), size_t(0));
        QCOMPARE(touch.count(EV_ABS, ABS_MT_TRACKING_ID), 2);   // down and up
        QCOMPARE(touch.count(EV_ABS, ABS_MT_POSITION_X), 2);
        QCOMPARE(touch.count(EV_KEY, BTN_TOUCH), 2);
    }

    void slidingRightAlongTheStripScrollsUp()
    {
        down(0, 1, 100, 50);
        moveTo(0, 200, 52);   // 10 mm to the right, 5 mm per notch
        QCOMPARE(wheel.sum(EV_REL, REL_WHEEL), 2);
        QCOMPARE(wheel.sum(EV_REL, REL_WHEEL_HI_RES), 240);
        up(0);
        QCOMPARE(touch.events.size(), size_t(0));   // the cursor never saw this touch
    }

    void slidingLeftScrollsDown()
    {
        down(0, 1, 500, 50);
        moveTo(0, 450, 50);
        QCOMPARE(wheel.sum(EV_REL, REL_WHEEL), -1);
    }

    void invertSwapsTheDirection()
    {
        makeScroller({12, 5, true});
        down(0, 1, 100, 50);
        moveTo(0, 200, 50);
        QCOMPARE(wheel.sum(EV_REL, REL_WHEEL), -2);
    }

    void fasterSettingScrollsMore()
    {
        makeScroller({12, 2.5, false});
        down(0, 1, 100, 50);
        moveTo(0, 200, 50);
        QCOMPARE(wheel.sum(EV_REL, REL_WHEEL), 4);
    }

    void smallMovementsAccumulateIntoNotches()
    {
        down(0, 1, 100, 50);
        for (int i = 1; i <= 10; ++i)
            moveTo(0, 100 + i * 10, 50);   // ten 1 mm steps
        QCOMPARE(wheel.sum(EV_REL, REL_WHEEL), 2);
        QCOMPARE(wheel.sum(EV_REL, REL_WHEEL_HI_RES), 240);
    }

    void stripHeightFollowsTheOption()
    {
        makeScroller({4, 5, false});   // strip is now the top 40 units
        down(0, 1, 100, 80);
        moveTo(0, 200, 80);
        QCOMPARE(wheel.events.size(), size_t(0));
        QVERIFY(touch.count(EV_ABS, ABS_MT_POSITION_X) > 0);
    }

    void optionsCanChangeWhileRunning()
    {
        s->setOptions({4, 5, false});
        down(0, 1, 100, 80);
        QVERIFY(wheel.events.empty());
        up(0);
        s->setOptions({20, 5, false});
        down(0, 2, 100, 80);
        moveTo(0, 200, 80);
        QCOMPARE(wheel.sum(EV_REL, REL_WHEEL), 2);
    }

    void secondFingerAfterAnOrdinaryTouchIsNotCaptured()
    {
        down(0, 1, 500, 400);
        down(1, 2, 300, 50);   // in the strip, but another finger is already down
        moveTo(1, 400, 50);
        QCOMPARE(wheel.events.size(), size_t(0));
        QCOMPARE(touch.count(EV_KEY, BTN_TOOL_DOUBLETAP), 2);   // set on, and the single-tap tool cleared
    }

    void touchThatStartsOutsideStaysOrdinaryWhenItEntersTheStrip()
    {
        down(0, 1, 500, 400);
        moveTo(0, 520, 50);
        moveTo(0, 620, 50);
        QCOMPARE(wheel.events.size(), size_t(0));
    }

    void physicalClicksStillPassThrough()
    {
        s->handle(EV_KEY, BTN_LEFT, 1);
        frame();
        QCOMPARE(touch.count(EV_KEY, BTN_LEFT), 1);
    }

    void tapInTheStripIsStillForwardedWhenItNeverMoves()
    {
        // Released before a full frame placed it: it must not leave a stuck touch behind.
        s->handle(EV_ABS, ABS_MT_SLOT, 0);
        s->handle(EV_ABS, ABS_MT_TRACKING_ID, 3);
        s->handle(EV_ABS, ABS_MT_TRACKING_ID, -1);
        frame();
        QCOMPARE(touch.count(EV_ABS, ABS_MT_TRACKING_ID), 2);
    }

    void droppedEventsReleaseForwardedTouches()
    {
        down(0, 1, 500, 400);
        touch.events.clear();
        s->handle(EV_SYN, SYN_DROPPED, 0);
        QCOMPARE(touch.sum(EV_ABS, ABS_MT_TRACKING_ID), -1);
        QCOMPARE(touch.sum(EV_KEY, BTN_TOUCH), 0);
    }

    // --- cruise: 100 x 70 mm pad, 10 units per mm; strip is the top 12 mm ---

    void cruiseIsOffByDefault()
    {
        down(0, 1, 500, 50);
        moveTo(0, 800, 50);
        wheel.events.clear();
        s->tick(1.0);
        QCOMPARE(wheel.events.size(), size_t(0));
        QVERIFY(!s->cruising());
    }

    void cruiseKeepsScrollingWhileTheFingerRests()
    {
        makeScroller({12, 5, false, true});
        down(0, 1, 500, 50);
        moveTo(0, 700, 50);   // 20 mm right
        QCOMPARE(wheel.sum(EV_REL, REL_WHEEL), 0);   // moving the finger doesn't scroll directly
        QVERIFY(s->cruising());
        // (5 mm head start + 20 mm - 1.5 mm dead zone) / 5 mm per notch * 2 = 9.4 notches per second
        QCOMPARE(s->cruiseRate(), 9.4);
        for (int i = 0; i < 100; ++i)
            s->tick(0.01);   // the finger doesn't move for a second
        QCOMPARE(wheel.sum(EV_REL, REL_WHEEL), 9);
    }

    void cruiseGoesFasterTheFurtherYouSlide()
    {
        makeScroller({12, 5, false, true});
        down(0, 1, 100, 50);
        moveTo(0, 300, 50);
        const double near = s->cruiseRate();
        moveTo(0, 500, 50);
        QVERIFY(s->cruiseRate() > near);
        QVERIFY(near > 0);
    }

    void cruiseSlowsAndReversesWhenYouSlideBack()
    {
        makeScroller({12, 5, false, true});
        down(0, 1, 500, 50);
        moveTo(0, 800, 50);
        const double fast = s->cruiseRate();
        moveTo(0, 650, 50);
        QVERIFY(s->cruiseRate() < fast);
        QVERIFY(s->cruiseRate() > 0);
        moveTo(0, 500, 50);
        QCOMPARE(s->cruiseRate(), 0.0);
        moveTo(0, 300, 50);
        QVERIFY(s->cruiseRate() < 0);   // past where it landed, the other way: scrolls down
    }

    void cruiseIgnoresSmallMovements()
    {
        makeScroller({12, 5, false, true});
        down(0, 1, 500, 50);
        moveTo(0, 514, 50);   // 1.4 mm: inside the dead zone
        QCOMPARE(s->cruiseRate(), 0.0);
        QVERIFY(!s->cruising());
        moveTo(0, 486, 50);   // and the same on the other side
        QCOMPARE(s->cruiseRate(), 0.0);
    }

    void cruiseStartsAtOnceJustPastTheDeadZone()
    {
        makeScroller({12, 5, false, true});
        down(0, 1, 500, 50);
        moveTo(0, 520, 50);   // 2 mm: 0.5 mm past the dead zone
        QVERIFY(s->cruiseRate() >= 2.0);   // already a clear scroll, not a crawl
        moveTo(0, 480, 50);
        QVERIFY(s->cruiseRate() <= -2.0);
    }

    void cruiseStopsWhenTheFingerLifts()
    {
        makeScroller({12, 5, false, true});
        down(0, 1, 500, 50);
        moveTo(0, 800, 50);
        QVERIFY(s->cruising());
        up(0);
        QVERIFY(!s->cruising());
        wheel.events.clear();
        s->tick(1.0);
        QCOMPARE(wheel.events.size(), size_t(0));
    }

    void cruiseRespectsReverseDirection()
    {
        makeScroller({12, 5, true, true});
        down(0, 1, 500, 50);
        moveTo(0, 700, 50);
        QVERIFY(s->cruiseRate() < 0);
    }

    void cruiseSpeedFollowsTheSpeedSetting()
    {
        makeScroller({12, 5, false, true});
        down(0, 1, 500, 50);
        moveTo(0, 700, 50);
        const double slow = s->cruiseRate();
        makeScroller({12, 2.5, false, true});
        down(0, 1, 500, 50);
        moveTo(0, 700, 50);
        // the starting speed is its own setting; the speed setting scales the growth beyond it
        QCOMPARE(s->cruiseRate() - 2.0, (slow - 2.0) * 2);
    }

    void cruiseHasACeiling()
    {
        makeScroller({12, 2, false, true});
        down(0, 1, 0, 50);
        moveTo(0, 1000, 50);   // the whole width of the pad at the fastest setting
        QVERIFY(s->cruiseRate() <= 60.0);
    }

    void cruiseDeadZoneStartSpeedAndCeilingAreSettings()
    {
        ScrollOptions o{12, 5, false, true};
        o.deadMm = 4;
        makeScroller(o);
        down(0, 1, 500, 50);
        moveTo(0, 535, 50);   // 3.5 mm: now inside a 4 mm dead zone
        QCOMPARE(s->cruiseRate(), 0.0);
        moveTo(0, 545, 50);   // 0.5 mm past it: just the start speed
        QCOMPARE(s->cruiseRate(), 2.0 + 0.5 * 0.2 * 2);

        o.deadMm = 1.5;
        o.cruiseStart = 6;
        makeScroller(o);
        down(0, 1, 500, 50);
        moveTo(0, 520, 50);
        QVERIFY(s->cruiseRate() >= 6.0);

        o.cruiseMax = 10;
        makeScroller(o);
        down(0, 1, 0, 50);
        moveTo(0, 1000, 50);
        QCOMPARE(s->cruiseRate(), 10.0);
    }

    void cruiseNeverStartsFromOutsideTheStrip()
    {
        makeScroller({12, 5, false, true});
        down(0, 1, 500, 400);
        moveTo(0, 900, 400);
        QVERIFY(!s->cruising());
    }

    void configRoundTripsCruise()
    {
        QTemporaryDir dir;
        Config c;
        c.cruise = true;
        QVERIFY(c.save(dir.filePath("c.conf")));
        QVERIFY(Config::load(dir.filePath("c.conf")).cruise);
        QVERIFY(!Config::load(dir.filePath("missing.conf")).cruise);   // off unless asked for
    }

    void padSizeComesFromTheResolution()
    {
        QCOMPARE(Scroller::extentMm({0, 1000, 10}), 100.0);
        QCOMPARE(Scroller::extentMm({0, 1000, 0}), 0.0);
    }

    void configRoundTrips()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("nested/topscroll.conf");
        Config c;
        c.enabled = false;
        c.stripMm = 17;
        c.mmPerNotch = 3.5;
        c.invert = true;
        c.cruise = true;
        c.deadMm = 3;
        c.cruiseStart = 4.5;
        c.cruiseMax = 90;
        QVERIFY(c.save(path));
        QCOMPARE(Config::load(path), c);
    }

    void configDefaultsAndClamps()
    {
        QTemporaryDir dir;
        QCOMPARE(Config::load(dir.filePath("missing.conf")), Config{});
        writeFile(dir.filePath("wild.conf"), "[General]\nstrip_mm=500\nmm_per_notch=0\n");
        const Config c = Config::load(dir.filePath("wild.conf"));
        QCOMPARE(c.stripMm, Config::kMaxStripMm);
        QCOMPARE(c.mmPerNotch, Config::kMinMmPerNotch);
        writeFile(dir.filePath("wild2.conf"), "[General]\ndead_mm=99\ncruise_start=0\ncruise_max=5000\n");
        const Config d = Config::load(dir.filePath("wild2.conf"));
        QCOMPARE(d.deadMm, Config::kMaxDeadMm);
        QCOMPARE(d.cruiseStart, Config::kMinCruiseStart);
        QCOMPARE(d.cruiseMax, Config::kMaxCruiseMax);
    }

    void statusRoundTripsAndNoticesDeadProcesses()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("run/status.json");
        Status st;
        st.state = "active";
        st.device = "Fancy Touchpad";
        st.pid = QCoreApplication::applicationPid();
        st.startedMs = 1234;
        st.exe = "/usr/bin/topscroll";
        st.widthMm = 110;
        st.heightMm = 70;
        QVERIFY(st.save(path));
        QCOMPARE(Status::load(path), st);
        QVERIFY(st.alive());
        st.pid = 0x7ffffff0;   // beyond any real pid_max
        QVERIFY(!st.alive());
    }

    void anUpgradedBinaryMeansTheDaemonNeedsARestart()
    {
        QTemporaryDir dir;
        const QString exe = dir.filePath("topscroll");
        writeFile(exe, "binary");
        const qint64 now = QDateTime::currentMSecsSinceEpoch();

        Status st;
        st.state = "active";
        st.pid = QCoreApplication::applicationPid();
        st.exe = exe;

        st.startedMs = now + 60'000;   // started after the binary was installed
        QVERIFY(!st.needsRestart());

        st.startedMs = now - 3'600'000;   // started an hour before the binary was replaced
        QVERIFY(st.needsRestart());

        st.startedMs = 0;   // an older daemon that never recorded when it started
        QVERIFY(st.needsRestart());

        st.pid = 0x7ffffff0;   // not running at all: nothing to restart
        QVERIFY(!st.needsRestart());
    }

    void themeReadsTheAccent()
    {
        QCOMPARE(Theme::parseAccent("background = \"#000000\"\naccent = \"#82FB9C\"\n"), QString("#82FB9C"));
        QCOMPARE(Theme::parseAccent("nothing here"), QString(Theme::kFallbackAccent));
    }

    void themeFollowsAFileThatIsReplaced()
    {
        QTemporaryDir dir;
        const QString file = dir.filePath("current/theme/colors.toml");
        writeFile(file, "accent = \"#112233\"\n");
        Theme theme(file);
        QCOMPARE(theme.accent(), QString("#112233"));

        QSignalSpy spy(&theme, &Theme::accentChanged);
        // A theme switch writes a new file and renames it over the old one.
        writeFile(file + ".new", "accent = \"#445566\"\n");
        QVERIFY(QFile::remove(file));
        QVERIFY(QFile::rename(file + ".new", file));
        QTRY_COMPARE(theme.accent(), QString("#445566"));
        QVERIFY(spy.count() >= 1);

        // And it keeps following after the first replacement.
        writeFile(file + ".new", "accent = \"#778899\"\n");
        QVERIFY(QFile::remove(file));
        QVERIFY(QFile::rename(file + ".new", file));
        QTRY_COMPARE(theme.accent(), QString("#778899"));
    }

    void themeFallsBackWithoutOmarchy()
    {
        QTemporaryDir dir;
        Theme theme(dir.filePath("nope/colors.toml"));
        QCOMPARE(theme.accent(), QString(Theme::kFallbackAccent));
    }

    void translatorFallsBackFromRegionToLanguageToEnglish()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("i18n/pt.json"), R"({"Save": "Guardar", "Quit": "Sair"})");
        writeFile(dir.filePath("i18n/pt-BR.json"), R"({"Save": "Salvar"})");
        writeFile(dir.filePath("language"), "pt-BR\n");
        Translator t(dir.filePath("language"), dir.filePath("i18n"));
        QCOMPARE(t.language(), QString("pt-BR"));
        QCOMPARE(t.translate("", "Save", nullptr, -1), QString("Salvar"));   // regional wins
        QCOMPARE(t.translate("", "Quit", nullptr, -1), QString("Sair"));     // per-string fallback to pt
        QCOMPARE(t.translate("", "Open", nullptr, -1), QString());           // untranslated: English stays
    }

    void translatorFollowsALanguageSwitch()
    {
        QTemporaryDir dir;
        writeFile(dir.filePath("i18n/da.json"), R"({"Quit": "Afslut"})");
        writeFile(dir.filePath("i18n/de.json"), R"({"Quit": "Beenden"})");
        writeFile(dir.filePath("language"), "da\n");
        Translator t(dir.filePath("language"), dir.filePath("i18n"));
        QCOMPARE(t.translate("", "Quit", nullptr, -1), QString("Afslut"));

        QSignalSpy spy(&t, &Translator::languageChanged);
        writeFile(dir.filePath("language"), "de\n");
        QTRY_COMPARE(spy.count(), 1);
        QCOMPARE(t.translate("", "Quit", nullptr, -1), QString("Beenden"));
    }
};

QTEST_MAIN(TopscrollTests)
#include "topscroll_tests.moc"
