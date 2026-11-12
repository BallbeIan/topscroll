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
    }

    void statusRoundTripsAndNoticesDeadProcesses()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("run/status.json");
        Status st;
        st.state = "active";
        st.device = "Fancy Touchpad";
        st.pid = QCoreApplication::applicationPid();
        st.widthMm = 110;
        st.heightMm = 70;
        QVERIFY(st.save(path));
        QCOMPARE(Status::load(path), st);
        QVERIFY(st.alive());
        st.pid = 0x7ffffff0;   // beyond any real pid_max
        QVERIFY(!st.alive());
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
