#include "AppBootstrap.h"

#include "AnimStudio.h"
#include "AppController.h"
#include "AppPaths.h"
#include "AppSettings.h"
#include "BatchExporter.h"
#include "CollageStudio.h"
#include "ColorPicker.h"
#include "FolderModel.h"
#include "ImageProvider.h"
#include "LensController.h"
#include "MangaReader.h"
#include "PaneImageProvider.h"
#include "ThemeManager.h"
#include "ThumbnailImageProvider.h"
#include "FestiveManager.h"
#include "UpdateManager.h"
#include "VideoSupport.h"
#include "VideoToGif.h"
#include "Translations.h"

#include <QCoreApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QTimer>

namespace AppBootstrap {

Objects setup(QQmlApplicationEngine &engine)
{
    Objects objects;
    auto *imageProvider = new ImageProvider();
    engine.addImageProvider(QLatin1String("viewer"), imageProvider);

    auto *folderModel = new FolderModel(&engine);
    engine.rootContext()->setContextProperty("folderModel", folderModel);
    folderModel->thumbnailCache()->setDiskLocation(
        AppPaths::cacheDir() + QStringLiteral("/thumbnails"));

    auto *thumbnailProvider = new ThumbnailImageProvider(folderModel->thumbnailCache());
    engine.addImageProvider(QLatin1String("thumb"), thumbnailProvider);
    // The engine deletes its children (FolderModel and its thumbnail cache) before its image providers: stop the
    // thumbnail workers while the cache is still alive. (`destroyed` fires before the children go.)
    QObject::connect(&engine, &QObject::destroyed, QCoreApplication::instance(),
                     [thumbnailProvider]() { thumbnailProvider->shutdown(); }, Qt::DirectConnection);

    // The pictures of the "Varias imágenes" view: one per pane, each at the resolution it needs.
    auto *paneStore = new PaneImageStore(&engine);
    engine.addImageProvider(QLatin1String("pane"), new PaneImageProvider(paneStore));
    engine.rootContext()->setContextProperty("paneImages", paneStore);

    // The comic reader (qml/MangaReader.qml): a folder or .cbz read page by page; its pictures come from the pane provider.
    auto *mangaReader = new MangaReader(paneStore, &engine);
    engine.rootContext()->setContextProperty("mangaReader", mangaReader);

    // The animation studio (Crear > GIF animado): the list of pictures and options, and the pictures it shows.
    auto *animStudio = new AnimStudio(paneStore, &engine);
    engine.rootContext()->setContextProperty("animStudio", animStudio);
    engine.addImageProvider(QLatin1String("animprev"), new AnimPreviewProvider(animStudio));
    engine.addImageProvider(QLatin1String("animsrc"), new AnimThumbnailProvider(animStudio));

    // "Desde un vídeo" in the animation studio: reads the frames and hands them to the studio.
    auto *videoToGif = new VideoToGif(&engine);
    engine.rootContext()->setContextProperty("videoToGif", videoToGif);
    engine.addImageProvider(QLatin1String("videoprev"), new VideoPreviewProvider(videoToGif));
    QObject::connect(videoToGif, &VideoToGif::framesReady, animStudio,
                     [animStudio](const QList<QImage> &frames, int holdMs, const QString &label) {
                         animStudio->addFrames(frames, holdMs, label);
                     });

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

    // The holiday decorations (qml/FestiveDecor.qml).
    engine.rootContext()->setContextProperty("festive", new FestiveManager(appSettings, &engine));

    // Looks for a newer version on GitHub (qml/UpdateBanner.qml, Settings > Acerca de).
    auto *updater = new UpdateManager(appSettings, QCoreApplication::applicationVersion(), &engine);
    objects.updater = updater;
    engine.rootContext()->setContextProperty("updater", updater);
    if (!qEnvironmentVariableIsSet("PROJECTFOXY_NO_UPDATE_CHECK")) // the smoke test and the UI tests stay offline
        QTimer::singleShot(5000, updater, &UpdateManager::checkOnStart);

    // What video can be read (Qt Multimedia); the "video to GIF" screen will ask it.
    engine.rootContext()->setContextProperty("videoSupport", new VideoSupport(&engine));

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

    objects.imageProvider = imageProvider;
    objects.folderModel = folderModel;
    objects.controller = controller;
    objects.settings = appSettings;
    return objects;
}

} // namespace AppBootstrap
