#include "AppBootstrap.h"
#include "AppController.h"
#include "AppSettings.h"
#include "FontSubstitutions.h"
#include "SingleInstance.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QIcon>
#include <QStandardPaths>

#ifdef PROJECTFOXY_DEV_SERVER
#include "DevServer.h"
#endif

#ifdef Q_OS_WIN
#include "WindowsFrame.h"
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

    QGuiApplication app(argc, argv);
    installFontSubstitutions();
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

    QQmlApplicationEngine engine;

    // Everything the QML needs, owned by the engine (see AppBootstrap.h).
    AppController *controller = AppBootstrap::setup(engine).controller;

    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, []() { QCoreApplication::exit(-1); }, Qt::QueuedConnection);

    engine.loadFromModule("ImageViewerApp", "Main");

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
