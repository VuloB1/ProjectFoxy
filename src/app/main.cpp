#include "AppBootstrap.h"
#include "UpdateManager.h"
#include "VideoSupport.h"
#include "AppController.h"
#include "AppSettings.h"
#include "FontSubstitutions.h"
#include "SingleInstance.h"
#include "StartupWarmup.h"
#include "WindowGeometry.h"

#include <QGuiApplication>
#include <memory>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#ifdef PROJECTFOXY_DEV_SERVER
#include <QElapsedTimer>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QQmlComponent>
#endif
#include <QQuickStyle>
#include <QQuickWindow>
#include <QScreen>
#include <QTimer>
#include <QIcon>
#include <QStandardPaths>

#ifdef PROJECTFOXY_DEV_SERVER
#include "DevServer.h"
#include "StartupSampler.h"
#endif

#ifdef Q_OS_WIN
#include "WindowsFrame.h"
#include <windows.h>
#endif

#ifdef PROJECTFOXY_DEV_SERVER
// PROJECTFOXY_TRACE_STARTUP=1 prints, on stderr, how long after the process was created each step of the start is reached.
static void startupMark(const char *what)
{
    static const bool on = qEnvironmentVariableIsSet("PROJECTFOXY_TRACE_STARTUP");
    if (!on)
        return;
#ifdef Q_OS_WIN
    FILETIME created, exited, kernel, user, now;
    GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user);
    GetSystemTimeAsFileTime(&now);
    const auto ticks = [](const FILETIME &f) { return (quint64(f.dwHighDateTime) << 32) | f.dwLowDateTime; };
    fprintf(stderr, "STARTUP %7.1f ms  %s\n", double(ticks(now) - ticks(created)) / 10000.0, what);
    fflush(stderr);
#endif
}
#else
static inline void startupMark(const char *) {}
#endif

int main(int argc, char *argv[])
{
    // Render with Direct3D 11 unless the caller already chose a backend, so it
    // can be overridden from the environment (QSG_RHI_BACKEND=software, opengl,
    // vulkan...) to compare or to work around a driver, without rebuilding.
#ifdef Q_OS_WIN
    if (!qEnvironmentVariableIsSet("QSG_RHI_BACKEND"))
        qputenv("QSG_RHI_BACKEND", "d3d11");
#endif
    // (Elsewhere Qt picks the platform's backend: OpenGL on Linux, Metal on macOS.)

    startupMark("main()");
#ifdef PROJECTFOXY_DEV_SERVER
    // PROJECTFOXY_SAMPLE_STARTUP=1: sample the main thread from here to the first frame and print where the time went.
    if (qEnvironmentVariableIsSet("PROJECTFOXY_SAMPLE_STARTUP"))
        startSampling(1);
#endif
    QGuiApplication app(argc, argv);
    startupMark("QGuiApplication ready");
    installFontSubstitutions();
    StartupWarmup warmup; // fonts and the graphics driver, on a helper thread, while the interface is built
    app.setApplicationName("ProjectFoxy");
#ifndef Q_OS_WIN
    // Wayland matches a window to its .desktop file (icon, name, taskbar) by this ID, which is also the AppStream
    // and Flatpak ID (packaging/linux).
    app.setDesktopFileName(QStringLiteral("io.github.vulob1.ProjectFoxy"));
#endif
    app.setOrganizationName("ProjectFoxy");
    app.setApplicationVersion(QStringLiteral(PROJECTFOXY_VERSION)); // from project(VERSION) in CMakeLists.txt
    // These two names decide where the settings and the cache live (AppPaths.h migrates the
    // settings from the old "ImageViewer" folder). The window title is set in Main.qml.
    // qt_add_qml_module puts RESOURCES under the module's own prefix
    // (/qt/qml/<URI>/), the same place the .qml files live.
    app.setWindowIcon(QIcon(":/qt/qml/ImageViewerApp/resources/icons/icon.png"));

    QQuickStyle::setStyle("Basic"); // fully themable base for the custom Fluent-ish look

    // The file to open, if any: the first argument that is not an option. (QCoreApplication::arguments() is built
    // from the UTF-16 command line, so a name with characters outside the ANSI code page - Japanese, Cyrillic,
    // emoji... - arrives intact; argv[] would already have turned them into '?'.)
    QString fileArgument;
    bool newWindow = false;
    for (const QString &argument : app.arguments().mid(1)) {
        if (argument == QLatin1String("--new-window"))
            newWindow = true;
        else if (!argument.startsWith(QLatin1String("--")) && fileArgument.isEmpty())
            fileArgument = argument;
    }

    // One window: when Project Foxy is already running, hand it the file and leave. `--new-window` (or turning the
    // setting off) skips this. If the running one does not answer, this one runs on its own.
    SingleInstance single;
    if (!newWindow && AppSettings().singleInstance()) {
        const QStringList files = fileArgument.isEmpty() ? QStringList() : QStringList{fileArgument};
        const auto result = SingleInstance::sendToRunning(SingleInstance::defaultName(), files);
        if (result == SingleInstance::Result::Delivered)
            return 0;
        if (result == SingleInstance::Result::NobodyThere)
            single.listen();
    }

    // Qt Quick reads this when the window is created: without V-Sync a frame is presented as soon as it is ready.
    if (!qEnvironmentVariableIsSet("QSG_NO_VSYNC") && !AppSettings().verticalSync())
        qputenv("QSG_NO_VSYNC", "1");

    startupMark("before the engine");
    QQmlApplicationEngine engine;

    // Everything the QML needs, owned by the engine (see AppBootstrap.h).
    const AppBootstrap::Objects objects = AppBootstrap::setup(engine);
    AppController *controller = objects.controller;
#ifdef PROJECTFOXY_DEV_SERVER
    // Developer build: a fake GitHub (PROJECTFOXY_UPDATE_URL, PROJECTFOXY_UPDATE_BASE for the files) and no real
    // installer run (PROJECTFOXY_UPDATE_DRYRUN).
    if (qEnvironmentVariableIsSet("PROJECTFOXY_UPDATE_URL"))
        objects.updater->setApiUrl(QUrl(qEnvironmentVariable("PROJECTFOXY_UPDATE_URL")));
    if (qEnvironmentVariableIsSet("PROJECTFOXY_UPDATE_BASE"))
        objects.updater->setDownloadBase(qEnvironmentVariable("PROJECTFOXY_UPDATE_BASE"));
    objects.updater->setDryRun(qEnvironmentVariableIsSet("PROJECTFOXY_UPDATE_DRYRUN"));
#endif
    startupMark("AppBootstrap::setup done");
    // Building the window takes a while: decode the picture meanwhile instead of after it.
    if (!fileArgument.isEmpty() && !VideoSupport::looksLikeVideo(fileArgument))
        controller->prefetch(fileArgument);

    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, []() { QCoreApplication::exit(-1); }, Qt::QueuedConnection);

    // Open where the window was the last time, brought down to what the screens connected now can show.
    const AppSettings remembered;
    {
        QList<QRect> screens{QGuiApplication::primaryScreen()->availableGeometry()};
        for (const QScreen *screen : QGuiApplication::screens())
            if (screen != QGuiApplication::primaryScreen())
                screens.append(screen->availableGeometry());
        const QRect fit = fitWindowToScreens(remembered.windowRect(), screens, {480, 320}); // Main.qml's minimum size
        QVariantMap initial;
        if (!fit.isNull())
            initial = {{"x", fit.x()}, {"y", fit.y()}, {"width", fit.width()}, {"height", fit.height()}};
        if (remembered.windowMaximized())
            initial.insert("visibility", int(QWindow::Maximized));
        engine.setInitialProperties(initial);
    }

#ifdef PROJECTFOXY_DEV_SERVER
    // PROJECTFOXY_BENCH_QML=1 (optionally with PROJECTFOXY_BENCH_QML_NAMES="A,B"): how long each QML component takes to
    // load and to create on its own, cold, then exit. Run it with QT_QPA_PLATFORM=offscreen.
    if (qEnvironmentVariableIsSet("PROJECTFOXY_BENCH_QML")) {
        QStringList names = qEnvironmentVariable("PROJECTFOXY_BENCH_QML_NAMES").split(QLatin1Char('|'), Qt::SkipEmptyParts);
        if (names.isEmpty())
            names = {"AppButton", "TitleBar", "ThumbnailStrip", "ImageCanvas", "StatusBar", "UpdateBanner", "FestiveDecor",
                     "ConfirmDialog", "UnsavedChangesDialog", "OverwriteDialog", "SettingsDialog", "BatchExportDialog",
                     "BatchRenameDialog", "Toolbar", "EditPanel", "ReaderView", "MultiView", "GifStudio", "CollageMaker", "AboutPage"};
        if (qEnvironmentVariableIsSet("PROJECTFOXY_BENCH_PLATFORM")) {
            QElapsedTimer p;
            p.start();
            const int families = QFontDatabase::families().size();
            qWarning("BENCH fonts: %d families in %lld ms", families, p.restart());
            QFontMetrics fm{QFont(QStringLiteral("Segoe UI"), 10)};
            qWarning("BENCH first QFontMetrics: %lld ms (%d)", p.restart(), fm.height());
            QQuickWindow w;
            w.resize(800, 600);
            w.create();
            qWarning("BENCH QQuickWindow::create: %lld ms", p.restart());
            w.show();
            QCoreApplication::processEvents();
            qWarning("BENCH show + first events: %lld ms", p.restart());
            for (int i = 0; i < 20; ++i)
                QCoreApplication::processEvents();
            qWarning("BENCH 20 more event rounds: %lld ms", p.restart());
        }
        for (const QString &name : names) {
            QElapsedTimer t;
            t.start();
            QQmlComponent component(&engine);
            if (name.startsWith(QLatin1String("inline:")))
                component.setData((QStringLiteral("import QtQuick; import QtQuick.Controls; import ImageViewerApp; ") + name.mid(7)).toUtf8(), QUrl(QStringLiteral("qrc:/bench.qml")));
            else
                component.loadFromModule("ImageViewerApp", name);
            const qint64 load = t.restart();
            if (component.isError()) {
                fprintf(stderr, "BENCH %-22s ERROR %s\n", qPrintable(name), qPrintable(component.errorString().left(100)));
                continue;
            }
            QObject *object = qEnvironmentVariableIsSet("PROJECTFOXY_BENCH_HIDDEN") ? component.createWithInitialProperties({{"visible", false}}) : component.create();
            const qint64 create = t.elapsed();
            fprintf(stderr, "BENCH %-22s load %4lld ms   create %4lld ms\n", qPrintable(name), load, create);
            delete object;
        }
        return 0;
    }
#endif
    startupMark("before loading Main.qml");
    engine.loadFromModule("ImageViewerApp", "Main");
    startupMark("Main.qml loaded");

    if (engine.rootObjects().isEmpty())
        return -1;

#ifdef Q_OS_WIN
    // The window has no native frame (Main.qml), so give back what the frame gave: round corners, a shadow, snapping
    // to the screen edges, the Windows 11 snap layouts and the system resize (WindowsFrame.cpp).
#ifdef PROJECTFOXY_DEV_SERVER
    if (!qEnvironmentVariableIsSet("PROJECTFOXY_NO_NATIVE_FRAME")) // to compare against the plain frameless window
#endif
    if (auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first()))
        installWindowsFrame(window);
#endif

#ifdef PROJECTFOXY_DEV_SERVER
    if (qEnvironmentVariableIsSet("PROJECTFOXY_DEV_PORT"))
        if (auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first()))
            new DevServer(window, qEnvironmentVariableIntValue("PROJECTFOXY_DEV_PORT"), &app);
#endif

    // Remember the window for the next time: its normal rectangle (settled for a moment while it is not maximised or
    // minimised, so the maximised size never counts) and whether it was maximised, written when the program quits.
    if (auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first())) {
        struct Remembered { QRect normal; bool maximized = false; };
        auto *state = new Remembered{remembered.windowRect(), remembered.windowMaximized()};
        auto *settle = new QTimer(window);
        settle->setSingleShot(true);
        settle->setInterval(200);
        const auto note = [window, state]() {
            if (window->visibility() == QWindow::Windowed)
                state->normal = window->geometry();
        };
        QObject::connect(settle, &QTimer::timeout, window, note);
        for (auto signal : {&QWindow::xChanged, &QWindow::yChanged, &QWindow::widthChanged, &QWindow::heightChanged})
            QObject::connect(window, signal, settle, qOverload<>(&QTimer::start));
        QObject::connect(window, &QWindow::visibilityChanged, window, [window, state](QWindow::Visibility v) {
            if (v == QWindow::Maximized || v == QWindow::Windowed)
                state->maximized = v == QWindow::Maximized;
        });
        QObject::connect(&app, &QCoreApplication::aboutToQuit, window, [window, state, note]() {
            note();
            AppSettings().saveWindowState(state->normal, state->maximized);
            delete state;
        });
    }

#ifdef PROJECTFOXY_DEV_SERVER
    if (auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first())) {
        auto once = std::make_shared<bool>(false);
        QObject::connect(window, &QQuickWindow::frameSwapped, window, [once]() { if (!*once) { *once = true; startupMark("first frame shown"); if (qEnvironmentVariableIsSet("PROJECTFOXY_SAMPLE_STARTUP")) stopSamplingAndReport(70); } });
        QObject::connect(controller, &AppController::currentSourceChanged, window, []() { startupMark("picture source set"); });
        auto pictureLoaded = std::make_shared<bool>(false);
        QObject::connect(controller, &AppController::isLoadingChanged, window, [controller, pictureLoaded]() { if (!controller->isLoading()) { *pictureLoaded = true; startupMark("picture loaded"); } });
        QObject::connect(window, &QQuickWindow::frameSwapped, window, [pictureLoaded]() { static int after = 0; if (*pictureLoaded && after < 4) { ++after; startupMark("frame after the picture loaded"); } });
    }
#endif
    // Open-with / double-click association: the file in the arguments.
    if (!fileArgument.isEmpty())
        controller->openPath(fileArgument);

    // Another launch handed us a file, or just wants this window: open it (an unsaved edit raises the usual question)
    // and bring the window forward.
    QObject::connect(&single, &SingleInstance::openRequested, controller, &AppController::openPath);
    QObject::connect(&single, &SingleInstance::activateRequested, &app, [&engine]() {
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        if (!window)
            return;
        if (window->windowStates() & Qt::WindowMinimized)
            window->showNormal();
        window->raise();
        window->requestActivate();
    });

    return app.exec();
}
