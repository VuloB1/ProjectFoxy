#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>

class AppSettings;

// Says which holiday the window is decorated for (qml/FestiveDecor.qml reads `festival`; empty = none).
// It follows the date (core/Festivity) while "Adornos festivos" is on in Settings; `preview` shows a given holiday
// instead, for looking at them out of season (Settings > Apariencia; not saved, so it never outlives the session).
// PROJECTFOXY_FESTIVE=<key> starts with that preview, and PROJECTFOXY_FESTIVE=off keeps the date from decorating
// anything (the interface tests and the smoke test use it; a preview still shows).
class FestiveManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString festival READ festival NOTIFY festivalChanged)
    Q_PROPERTY(QString preview READ preview WRITE setPreview NOTIFY festivalChanged)

public:
    explicit FestiveManager(AppSettings *settings, QObject *parent = nullptr);

    QString festival() const { return m_festival; }
    QString preview() const { return m_preview; }
    // "" = by the date; a holiday's key = that one.
    void setPreview(const QString &key);

signals:
    void festivalChanged();

private:
    void refresh();

    QPointer<AppSettings> m_settings;
    QString m_preview;
    QString m_festival;
    bool m_forcedOff = false;
    QTimer m_dayTimer; // the date changes while the program stays open
};
