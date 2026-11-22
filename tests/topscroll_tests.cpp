#include <QTemporaryDir>
#include <QtTest>

#include "../src/inputcodes.h"

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
    void makeTouchpadStyle(ScrollOptions o = {})
    {
        o.touchpadStyle = true;
        makeScroller(o);
        s->setSlotCount(5);   // synthetic fingers go in slots 4 and 3
    }
    // The last value of every ABS code per slot, as the touch output would hold it.
    static std::map<int, std::map<int, int>> slotState(const RecordingSink &sink)
    {
        std::map<int, std::map<int, int>> state;
        int slot = 0;
        for (const auto &e : sink.events) {
            if (e.type != EV_ABS)
                continue;
            if (e.code == ABS_MT_SLOT)
                slot = e.value;
            else
                state[slot][e.code] = e.value;
        }
        return state;
    }
    static int lastKey(const RecordingSink &sink, int code)
    {
        int v = -1;
        for (const auto &e : sink.events)
            if (e.type == EV_KEY && e.code == code)
                v = e.value;
        return v;
    }
    void ticks(int n, double seconds = 0.01)
    {
        for (int i = 0; i < n; ++i)
            s->tick(seconds);
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
        ScrollOptions o{12, 5, false, true};
        o.inertia = false;
        makeScroller(o);
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

    // --- inertia: 100 x 70 mm pad, 10 units per mm; 5 mm per notch ---

    // Slides right at 40 mm/s (2 mm every 50 ms) for `frames` frames, ticking the clock as it goes.
    void slide(int frames, int dx = 20)
    {
        int x = 100;
        down(0, 1, x, 50);
        for (int f = 0; f < frames; ++f) {
            x += dx;
            moveTo(0, x, 50);
            for (int t = 0; t < 5; ++t)
                s->tick(0.01);
        }
    }
    void settle(double seconds)
    {
        for (int t = 0; t < int(seconds * 100); ++t)
            s->tick(0.01);
    }

    void scrollingGlidesAfterTheFingerLifts()
    {
        slide(10);
        up(0);
        wheel.events.clear();
        s->tick(0.01);
        QVERIFY(wheel.sum(EV_REL, REL_WHEEL_HI_RES) > 0);   // still moving up after lifting
        settle(5);
        const int total = wheel.sum(EV_REL, REL_WHEEL_HI_RES);
        QVERIFY(total > 120);                                // a real glide, not a twitch
        wheel.events.clear();
        settle(2);
        QCOMPARE(wheel.events.size(), size_t(0));            // and it comes to a stop
        QVERIFY(!s->needsTick());
    }

    void glideKeepsTheDirection()
    {
        slide(10, -20);
        up(0);
        wheel.events.clear();
        settle(3);
        QVERIFY(wheel.sum(EV_REL, REL_WHEEL_HI_RES) < 0);
    }

    void glideSlowsDown()
    {
        slide(10);
        up(0);
        wheel.events.clear();
        settle(0.3);
        const int first = wheel.sum(EV_REL, REL_WHEEL_HI_RES);
        wheel.events.clear();
        settle(0.3);
        QVERIFY(wheel.sum(EV_REL, REL_WHEEL_HI_RES) < first);
    }

    void aFingerThatPausedBeforeLiftingDoesNotGlide()
    {
        slide(10);
        settle(0.6);   // rests on the pad
        up(0);
        wheel.events.clear();
        settle(3);
        QCOMPARE(wheel.events.size(), size_t(0));
    }

    void noGlideWhenInertiaIsOff()
    {
        ScrollOptions o;
        o.inertia = false;
        makeScroller(o);
        slide(10);
        up(0);
        wheel.events.clear();
        settle(3);
        QCOMPARE(wheel.events.size(), size_t(0));
        QVERIFY(!s->needsTick());
    }

    void aLongerGlideTravelsFurther()
    {
        auto glideDistance = [&](double seconds) {
            ScrollOptions o;
            o.glideSeconds = seconds;
            makeScroller(o);
            slide(10);
            up(0);
            wheel.events.clear();
            settle(8);
            return wheel.sum(EV_REL, REL_WHEEL_HI_RES);
        };
        QVERIFY(glideDistance(2.0) > glideDistance(0.5) * 2);
    }

    void touchingTheStripAgainStopsTheGlide()
    {
        slide(10);
        up(0);
        settle(0.1);
        down(0, 2, 500, 400);   // a new finger anywhere
        wheel.events.clear();
        settle(1);
        QCOMPARE(wheel.events.size(), size_t(0));
    }

    void cruiseGlidesToAStopAfterLifting()
    {
        makeScroller({12, 5, false, true});
        down(0, 1, 500, 50);
        moveTo(0, 800, 50);
        const double rate = s->cruiseRate();
        QVERIFY(rate > 2);
        settle(0.2);
        up(0);
        wheel.events.clear();
        settle(0.1);
        QVERIFY(wheel.sum(EV_REL, REL_WHEEL_HI_RES) > 0);   // keeps going at roughly the cruise speed
        settle(5);
        wheel.events.clear();
        settle(1);
        QCOMPARE(wheel.events.size(), size_t(0));
    }

    void cruiseNeverStartsFromOutsideTheStrip()
    {
        makeScroller({12, 5, false, true});
        down(0, 1, 500, 400);
        moveTo(0, 900, 400);
        QVERIFY(!s->cruising());
    }

    // --- smooth scrolling ---

    void smoothScrollingSpreadsMovementOverTime()
    {
        ScrollOptions o;
        o.smooth = true;
        o.inertia = false;
        makeScroller(o);
        down(0, 1, 100, 50);
        moveTo(0, 200, 50);   // 10 mm: two notches' worth
        QCOMPARE(wheel.sum(EV_REL, REL_WHEEL_HI_RES), 0);   // waits for the clock
        QVERIFY(s->needsTick());
        s->tick(0.01);
        const int first = wheel.sum(EV_REL, REL_WHEEL_HI_RES);
        QVERIFY(first > 0 && first < 240);
        ticks(100);
        QCOMPARE(wheel.sum(EV_REL, REL_WHEEL_HI_RES), 240);   // all of it arrives, none lost
        QCOMPARE(wheel.sum(EV_REL, REL_WHEEL), 2);
        up(0);
        ticks(5);
        QVERIFY(!s->needsTick());
    }

    void longerSmoothingEasesOutMoreSlowly()
    {
        int sent[2];
        const double seconds[2] = {0.06, 0.3};
        for (int i = 0; i < 2; ++i) {
            ScrollOptions o;
            o.smooth = true;
            o.smoothSeconds = seconds[i];
            makeScroller(o);
            down(0, 1, 100, 50);
            moveTo(0, 200, 50);
            ticks(5);
            sent[i] = wheel.sum(EV_REL, REL_WHEEL_HI_RES);
        }
        QVERIFY(sent[0] > sent[1]);
    }

    void turningSmoothingOffSendsWhatIsWaiting()
    {
        ScrollOptions o;
        o.smooth = true;
        makeScroller(o);
        down(0, 1, 100, 50);
        moveTo(0, 200, 50);
        o.smooth = false;
        s->setOptions(o);
        QCOMPARE(wheel.sum(EV_REL, REL_WHEEL_HI_RES), 240);
    }

    void smoothingAlsoAppliesToCruise()
    {
        ScrollOptions o;
        o.smooth = true;
        o.cruise = true;
        makeScroller(o);
        down(0, 1, 500, 50);
        moveTo(0, 600, 50);
        s->tick(0.01);
        const double rate = s->cruiseRate();
        ticks(200);
        // After two seconds the eased output has caught up with the steady cruise speed.
        const int before = wheel.sum(EV_REL, REL_WHEEL_HI_RES);
        ticks(100);
        const int perSecond = wheel.sum(EV_REL, REL_WHEEL_HI_RES) - before;
        QVERIFY(qAbs(perSecond - rate * 120) <= 2);
    }

    // --- touchpad-style scrolling: synthetic fingers in slots 4 and 3 ---

    void touchpadStyleTurnsTheStripIntoATwoFingerSwipe()
    {
        makeTouchpadStyle();
        down(0, 1, 100, 50);
        auto st = slotState(touch);
        QVERIFY(st[4][ABS_MT_TRACKING_ID] >= 0);
        QVERIFY(st[3][ABS_MT_TRACKING_ID] >= 0);
        QVERIFY(!st.count(0));   // the real finger itself is never forwarded
        QCOMPARE(lastKey(touch, BTN_TOOL_DOUBLETAP), 1);
        QCOMPARE(lastKey(touch, BTN_TOUCH), 1);
        const int y0 = st[4][ABS_MT_POSITION_Y];
        QCOMPARE(y0, 350);   // landed in the middle of the pad

        moveTo(0, 200, 50);   // 10 mm right scrolls up: both fingers go 10 mm up the pad
        st = slotState(touch);
        QCOMPARE(st[4][ABS_MT_POSITION_Y], y0 - 100);
        QCOMPARE(st[3][ABS_MT_POSITION_Y], y0 - 100);
        QVERIFY(wheel.events.empty());

        up(0);
        st = slotState(touch);
        QCOMPARE(st[4][ABS_MT_TRACKING_ID], -1);
        QCOMPARE(st[3][ABS_MT_TRACKING_ID], -1);
        QCOMPARE(lastKey(touch, BTN_TOUCH), 0);
        ticks(50, 0.02);
        QVERIFY(wheel.events.empty());   // coasting is up to the app now
    }

    void touchpadStyleFollowsInvertAndSpeed()
    {
        ScrollOptions o;
        o.invert = true;
        o.mmPerNotch = 2.5;   // twice the default speed
        makeTouchpadStyle(o);
        down(0, 1, 100, 50);
        moveTo(0, 200, 50);
        QCOMPARE(slotState(touch)[4][ABS_MT_POSITION_Y], 350 + 200);
    }

    void touchpadStyleLandsAgainWhenItRunsOutOfRoom()
    {
        makeTouchpadStyle();
        down(0, 1, 50, 50);
        moveTo(0, 350, 50);   // 30 mm: more than the room above the middle
        QCOMPARE(slotState(touch)[4][ABS_MT_TRACKING_ID], -1);
        QCOMPARE(lastKey(touch, BTN_TOUCH), 0);
        moveTo(0, 351, 50);   // next frame: down again in the middle
        auto st = slotState(touch);
        QVERIFY(st[4][ABS_MT_TRACKING_ID] >= 0);
        QCOMPARE(st[4][ABS_MT_POSITION_Y], 350);
        moveTo(0, 352, 50);   // and the movement that didn't fit carries on
        QVERIFY(slotState(touch)[4][ABS_MT_POSITION_Y] < 350 - 200);
        QVERIFY(wheel.events.empty());
    }

    void touchpadStyleHandsOverToTheWheelWhenAnotherFingerLands()
    {
        makeTouchpadStyle();
        down(0, 1, 100, 50);
        down(1, 2, 500, 400);   // an ordinary second finger
        auto st = slotState(touch);
        QCOMPARE(st[4][ABS_MT_TRACKING_ID], -1);
        QCOMPARE(st[3][ABS_MT_TRACKING_ID], -1);
        QCOMPARE(st[1][ABS_MT_TRACKING_ID], 2);
        QCOMPARE(lastKey(touch, BTN_TOOL_FINGER), 1);   // one finger, not three
        moveTo(0, 200, 50);
        QCOMPARE(wheel.sum(EV_REL, REL_WHEEL), 2);
    }

    void touchpadStyleStepsAsideWhenAnotherFingerLandsTogether()
    {
        makeTouchpadStyle();
        s->handle(EV_ABS, ABS_MT_SLOT, 0);
        s->handle(EV_ABS, ABS_MT_TRACKING_ID, 1);
        s->handle(EV_ABS, ABS_MT_POSITION_X, 100);
        s->handle(EV_ABS, ABS_MT_POSITION_Y, 50);    // in the strip
        s->handle(EV_ABS, ABS_MT_SLOT, 1);
        s->handle(EV_ABS, ABS_MT_TRACKING_ID, 2);
        s->handle(EV_ABS, ABS_MT_POSITION_X, 500);
        s->handle(EV_ABS, ABS_MT_POSITION_Y, 400);   // an ordinary finger, same frame
        frame();
        QVERIFY(!slotState(touch).count(4));
        QCOMPARE(lastKey(touch, BTN_TOOL_FINGER), 1);
        moveTo(0, 200, 50);
        QCOMPARE(wheel.sum(EV_REL, REL_WHEEL), 2);
    }

    void touchpadStyleNeedsSlotsToBorrow()
    {
        ScrollOptions o;
        o.touchpadStyle = true;
        makeScroller(o);   // slot count unknown, as on Windows
        down(0, 1, 100, 50);
        moveTo(0, 200, 50);
        QCOMPARE(wheel.sum(EV_REL, REL_WHEEL), 2);
        QVERIFY(touch.events.empty());
    }

    void touchpadStyleCopiesPressureOntoTheFingers()
    {
        makeTouchpadStyle();
        s->handle(EV_ABS, ABS_MT_SLOT, 0);
        s->handle(EV_ABS, ABS_MT_TRACKING_ID, 1);
        s->handle(EV_ABS, ABS_MT_POSITION_X, 100);
        s->handle(EV_ABS, ABS_MT_POSITION_Y, 50);
        s->handle(EV_ABS, ABS_MT_PRESSURE, 40);
        frame();
        auto st = slotState(touch);
        QCOMPARE(st[4][ABS_MT_PRESSURE], 40);
        QCOMPARE(st[3][ABS_MT_PRESSURE], 40);
        s->handle(EV_ABS, ABS_MT_PRESSURE, 55);
        frame();
        QCOMPARE(slotState(touch)[3][ABS_MT_PRESSURE], 55);
    }

    void touchpadStyleStepsAsideForCruise()
    {
        ScrollOptions o;
        o.cruise = true;
        makeTouchpadStyle(o);
        down(0, 1, 500, 50);
        moveTo(0, 600, 50);
        ticks(10);
        QVERIFY(wheel.sum(EV_REL, REL_WHEEL_HI_RES) > 0);
        QVERIFY(touch.events.empty());
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
        c.inertia = false;
        c.glideSeconds = 1.8;
        c.smooth = false;
        c.smoothSeconds = 0.3;
        c.touchpadStyle = true;
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
        writeFile(dir.filePath("wild3.conf"), "[General]\nglide_seconds=99\n");
        QCOMPARE(Config::load(dir.filePath("wild3.conf")).glideSeconds, Config::kMaxGlideSeconds);
        QVERIFY(Config::load(dir.filePath("missing.conf")).inertia);   // on unless turned off
        QVERIFY(Config::load(dir.filePath("missing.conf")).smooth);    // on unless turned off
        QVERIFY(!Config::load(dir.filePath("missing.conf")).touchpadStyle);
        writeFile(dir.filePath("wild4.conf"), "[General]\nsmooth_seconds=9\n");
        QCOMPARE(Config::load(dir.filePath("wild4.conf")).smoothSeconds, Config::kMaxSmoothSeconds);
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
