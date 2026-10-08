#include "AppPaths.h"
#include "AppController.h"
#include "ThemeManager.h"
#include "AppSettings.h"
#include "ImageProvider.h"
#include "FolderModel.h"
#include "ThumbnailImageProvider.h"
#include "BatchExporter.h"
#include "LensController.h"
#include "PaneImageProvider.h"
#include "AnimStudio.h"
#include "CollageStudio.h"
#include "ColorPicker.h"
#include "Translations.h"
#include "FontSubstitutions.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QIcon>
#include <QStandardPaths>

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

    QQmlApplicationEngine engine;

    // All of the below are owned by the engine (parented to it) or by each
    // other; nothing here needs manual cleanup.
    auto *imageProvider = new ImageProvider();
    engine.addImageProvider(QLatin1String("viewer"), imageProvider);

    auto *folderModel = new FolderModel(&engine);
    engine.rootContext()->setContextProperty("folderModel", folderModel);
    folderModel->thumbnailCache()->setDiskLocation(
        AppPaths::cacheDir() + QStringLiteral("/thumbnails"));

    auto *thumbnailProvider = new ThumbnailImageProvider(folderModel->thumbnailCache());
    engine.addImageProvider(QLatin1String("thumb"), thumbnailProvider);

    // The pictures of the "Varias imágenes" view: one per pane, each at the resolution it needs.
    auto *paneStore = new PaneImageStore(&engine);
    engine.addImageProvider(QLatin1String("pane"), new PaneImageProvider(paneStore));
    engine.rootContext()->setContextProperty("paneImages", paneStore);

    // The animation studio (Crear > GIF animado): the list of pictures and options, and the pictures it shows.
    auto *animStudio = new AnimStudio(paneStore, &engine);
    engine.rootContext()->setContextProperty("animStudio", animStudio);
    engine.addImageProvider(QLatin1String("animprev"), new AnimPreviewProvider(animStudio));
    engine.addImageProvider(QLatin1String("animsrc"), new AnimThumbnailProvider(animStudio));

    // The collage maker (Crear > Collage).
    auto *collageStudio = new CollageStudio(paneStore, &engine);
    engine.rootContext()->setContextProperty("collageStudio", collageStudio);
    engine.addImageProvider(QLatin1String("collageprev"), new CollagePreviewProvider(collageStudio));

    auto *controller = new AppController(imageProvider, folderModel, &engine);
    engine.rootContext()->setContextProperty("appController", controller);

    auto *themeManager = new ThemeManager(&engine);
    engine.rootContext()->setContextProperty("themeManager", themeManager);

    auto *appSettings = new AppSettings(&engine);
    engine.rootContext()->setContextProperty("appSettings", appSettings);

    // The language: installed before any text is built (Main.qml's qsTr() are evaluated when it loads).
    auto *translations = new Translations(&engine, appSettings, &engine);
    engine.rootContext()->setContextProperty("translations", translations);

    auto *batchExporter = new BatchExporter(&engine);
    engine.rootContext()->setContextProperty("batchExporter", batchExporter);

    auto *colorPicker = new ColorPicker(&engine);
    engine.rootContext()->setContextProperty("colorPicker", colorPicker);

    auto *lensController = new LensController(&engine);
    engine.rootContext()->setContextProperty("lensController", lensController);

    QObject::connect(translations, &Translations::activeChanged, controller, &AppController::retranslate);

    // Filmstrip navigation (arrow keys, click, Toolbar prev/next) reports a
    // new current file here; AppController decodes and displays it.
    QObject::connect(folderModel, &FolderModel::currentFilePathChanged,
                      controller, &AppController::openPath);

    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, []() { QCoreApplication::exit(-1); }, Qt::QueuedConnection);

    engine.loadFromModule("ImageViewerApp", "Main");

    if (engine.rootObjects().isEmpty())
        return -1;

    // Open-with / double-click association: the first argument is the file path.
    // QCoreApplication::arguments() is built from the UTF-16 command line, so a
    // name with characters outside the ANSI code page (Japanese, Cyrillic,
    // emoji...) arrives intact; argv[] would already have turned them into '?'.
    const QStringList arguments = app.arguments();
    if (arguments.size() > 1)
        controller->openPath(arguments.at(1));

    return app.exec();
}
