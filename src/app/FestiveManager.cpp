#include "FestiveManager.h"

#include "AppSettings.h"
#include "Festivity.h"

#include <QDate>

FestiveManager::FestiveManager(AppSettings *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
{
    const QString env = qEnvironmentVariable("PROJECTFOXY_FESTIVE");
    if (env == QLatin1String("off"))
        m_forcedOff = true;
    else if (core::festivalFromKey(env) != core::Festival::None)
        m_preview = env;

    if (m_settings)
        connect(m_settings, &AppSettings::festiveChanged, this, &FestiveManager::refresh);
    m_dayTimer.setInterval(30 * 60 * 1000);
    connect(&m_dayTimer, &QTimer::timeout, this, &FestiveManager::refresh);
    m_dayTimer.start();
    refresh();
}

void FestiveManager::setPreview(const QString &key)
{
    const QString clean = core::festivalFromKey(key) == core::Festival::None ? QString() : key;
    if (clean == m_preview)
        return;
    m_preview = clean;
    refresh();
}

void FestiveManager::refresh()
{
    QString now;
    if (!m_preview.isEmpty())
        now = m_preview;
    else if (!m_forcedOff && (!m_settings || m_settings->festive()))
        now = core::festivalKey(core::festivalOn(QDate::currentDate()));
    if (now == m_festival)
        return;
    m_festival = now;
    emit festivalChanged();
}
