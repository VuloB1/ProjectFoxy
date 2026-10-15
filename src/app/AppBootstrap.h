#pragma once

class UpdateManager;
class AppController;
class AppSettings;
class FolderModel;
class ImageProvider;
class QQmlApplicationEngine;

// The wiring of the program: the image providers, the controllers and models, and the context properties the QML reads.
// main() and the interface tests both start from here, so the tests run on exactly what the program runs on.
namespace AppBootstrap {

struct Objects {
    ImageProvider *imageProvider = nullptr;
    FolderModel *folderModel = nullptr;
    AppController *controller = nullptr;
    AppSettings *settings = nullptr;
    UpdateManager *updater = nullptr;
};

// Creates everything and hands it to `engine` (which owns it all: nothing needs manual cleanup). Does not load Main.qml.
Objects setup(QQmlApplicationEngine &engine);

} // namespace AppBootstrap
