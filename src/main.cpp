#include <QCoreApplication>
#include <QGuiApplication>
#include <QLocale>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

#include <cstdio>

#include "backend.h"
#include "config.h"
#include "daemon.h"
#include "status.h"
#include "theme.h"
#include "translator.h"

namespace {

const char *kVersion = "0.1.0";

int usage(FILE *out, int code)
{
    std::fputs("Usage: topscroll [command]\n"
               "\n"
               "Scroll with one finger along the touchpad's top edge.\n"
               "\n"
               "Commands:\n"
               "  open            show the settings window (default)\n"
               "  daemon          run the scrolling service in the foreground\n"
               "  status [--json] report what the service is doing; exits 1 if it isn't running\n"
               "  --version       print the version\n",
               out);
    return code;
}

int runDaemon(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    Daemon daemon(Config::defaultPath(), Status::defaultPath());
    daemon.start();
    return app.exec();
}

int runStatus(int argc, char **argv, bool json)
{
    QCoreApplication app(argc, argv);
    const Status s = Status::load(Status::defaultPath());
    if (!s.alive()) {
        if (json)
            std::puts("{\"state\":\"stopped\"}");
        else
            std::fputs("stopped\n", stdout);
        std::fputs("topscroll: the service isn't running; start it with "
                   "`systemctl --user enable --now topscroll`\n", stderr);
        return 1;
    }
    if (json)
        std::printf("%s\n", s.toJson().constData());
    else
        std::printf("%s%s%s\n", qPrintable(s.state), s.device.isEmpty() ? "" : ": ",
                    qPrintable(s.device));
    return 0;
}

int runGui(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    app.setApplicationName("topscroll");
    app.setDesktopFileName("topscroll");
    QQuickStyle::setStyle("Material");

    Translator translator;
    QCoreApplication::installTranslator(&translator);
    const auto applyLocale = [&] {
        QLocale::setDefault(QLocale(QString(translator.language()).replace('-', '_')));
        app.setLayoutDirection(QLocale().textDirection());
    };
    applyLocale();

    Theme theme;
    theme.refreshRadius();
    Backend backend;

    QQmlApplicationEngine engine;
    QObject::connect(&translator, &Translator::languageChanged, &app, [&] {
        applyLocale();
        engine.retranslate();
    });
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.rootContext()->setContextProperty("theme", &theme);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.load(QUrl("qrc:/Main.qml"));
    return app.exec();
}

}

int main(int argc, char **argv)
{
    const QString command = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QStringLiteral("open");
    if (command == "open")
        return runGui(argc, argv);
    if (command == "daemon")
        return runDaemon(argc, argv);
    if (command == "status")
        return runStatus(argc, argv, argc > 2 && QString::fromLocal8Bit(argv[2]) == "--json");
    if (command == "--version" || command == "-v") {
        std::printf("topscroll %s\n", kVersion);
        return 0;
    }
    if (command == "--help" || command == "-h")
        return usage(stdout, 0);
    return usage(stderr, 2);
}
