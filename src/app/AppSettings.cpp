#include "AppSettings.h"

#include "AppPaths.h"

#include <QStandardPaths>
#include <algorithm>

namespace {
constexpr int kDefaultIntervalMs = 3000;
constexpr bool kDefaultRandom = false;
constexpr bool kDefaultLoop = true;
constexpr int kDefaultQuality = 90;
constexpr int kDefaultPadding = 3;
}

AppSettings::AppSettings(QObject *parent)
    : QObject(parent)
    , m_store(AppPaths::configDir() + QStringLiteral("/settings.ini"),
               QSettings::IniFormat)
{
    m_slideshowIntervalMs = m_store.value(QStringLiteral("slideshow/intervalMs"), kDefaultIntervalMs).toInt();
    m_slideshowRandom = m_store.value(QStringLiteral("slideshow/random"), kDefaultRandom).toBool();
    m_slideshowLoop = m_store.value(QStringLiteral("slideshow/loop"), kDefaultLoop).toBool();
    m_batchExportQuality = m_store.value(QStringLiteral("batch/exportQuality"), kDefaultQuality).toInt();
    m_batchRenamePadding = m_store.value(QStringLiteral("batch/renamePadding"), kDefaultPadding).toInt();
    m_confirmOverwrite = m_store.value(QStringLiteral("save/confirmOverwrite"), true).toBool();

    // The mode started life as one all-in-one switch ("view/pixelArt"). Whoever had that on keeps
    // every extra on; for everybody else the extras start off.
    const bool wasAllInOne = m_store.value(QStringLiteral("view/pixelArt"), false).toBool();
    m_pixelMode = m_store.value(QStringLiteral("view/pixelMode"), wasAllInOne).toBool();
    m_pixelIntegerZoom = m_store.value(QStringLiteral("view/pixelIntegerZoom"), wasAllInOne).toBool();
    m_pixelCheckerboard = m_store.value(QStringLiteral("view/pixelCheckerboard"), wasAllInOne).toBool();
    m_pixelGrid = m_store.value(QStringLiteral("view/pixelGrid"), wasAllInOne).toBool();
    m_pixelReadout = m_store.value(QStringLiteral("view/pixelReadout"), wasAllInOne).toBool();
    m_pixelSharpEdit = m_store.value(QStringLiteral("view/pixelSharpEdit"), wasAllInOne).toBool();
}

bool AppSettings::storeFlag(bool &member, const QString &key, bool value)
{
    if (member == value)
        return false;
    member = value;
    m_store.setValue(key, value);
    return true;
}

void AppSettings::setSlideshowIntervalMs(int value)
{
    value = std::clamp(value, 200, 60000);
    if (m_slideshowIntervalMs == value)
        return;
    m_slideshowIntervalMs = value;
    m_store.setValue(QStringLiteral("slideshow/intervalMs"), value);
    emit slideshowIntervalMsChanged();
}

void AppSettings::setSlideshowRandom(bool value)
{
    if (m_slideshowRandom == value)
        return;
    m_slideshowRandom = value;
    m_store.setValue(QStringLiteral("slideshow/random"), value);
    emit slideshowRandomChanged();
}

void AppSettings::setPixelMode(bool value)
{
    if (storeFlag(m_pixelMode, QStringLiteral("view/pixelMode"), value))
        emit pixelModeChanged();
}

void AppSettings::setPixelIntegerZoom(bool value)
{
    if (storeFlag(m_pixelIntegerZoom, QStringLiteral("view/pixelIntegerZoom"), value))
        emit pixelIntegerZoomChanged();
}

void AppSettings::setPixelCheckerboard(bool value)
{
    if (storeFlag(m_pixelCheckerboard, QStringLiteral("view/pixelCheckerboard"), value))
        emit pixelCheckerboardChanged();
}

void AppSettings::setPixelGrid(bool value)
{
    if (storeFlag(m_pixelGrid, QStringLiteral("view/pixelGrid"), value))
        emit pixelGridChanged();
}

void AppSettings::setPixelReadout(bool value)
{
    if (storeFlag(m_pixelReadout, QStringLiteral("view/pixelReadout"), value))
        emit pixelReadoutChanged();
}

void AppSettings::setPixelSharpEdit(bool value)
{
    if (storeFlag(m_pixelSharpEdit, QStringLiteral("view/pixelSharpEdit"), value))
        emit pixelSharpEditChanged();
}

void AppSettings::setSlideshowLoop(bool value)
{
    if (m_slideshowLoop == value)
        return;
    m_slideshowLoop = value;
    m_store.setValue(QStringLiteral("slideshow/loop"), value);
    emit slideshowLoopChanged();
}

void AppSettings::setBatchExportQuality(int value)
{
    value = std::clamp(value, 1, 100);
    if (m_batchExportQuality == value)
        return;
    m_batchExportQuality = value;
    m_store.setValue(QStringLiteral("batch/exportQuality"), value);
    emit batchExportQualityChanged();
}

void AppSettings::setConfirmOverwrite(bool value)
{
    if (m_confirmOverwrite == value)
        return;
    m_confirmOverwrite = value;
    m_store.setValue(QStringLiteral("save/confirmOverwrite"), value);
    emit confirmOverwriteChanged();
}

void AppSettings::setBatchRenamePadding(int value)
{
    value = std::clamp(value, 1, 6);
    if (m_batchRenamePadding == value)
        return;
    m_batchRenamePadding = value;
    m_store.setValue(QStringLiteral("batch/renamePadding"), value);
    emit batchRenamePaddingChanged();
}
