#pragma once

#include <QHash>
#include <QLocale>
#include <QObject>
#include <QString>
#include <QTranslator>
#include <QVariantList>

class QQmlEngine;
class AppSettings;

// A translator that looks the Spanish text up in a JSON catalogue ({"Spanish": "translation"}). The Spanish text
// is the key everywhere - qsTr() in QML, tr() / core::tr() in C++ - so the context is ignored.
class CatalogTranslator : public QTranslator {
public:
    bool loadJson(const QString &path);
    bool loadJsonData(const QByteArray &json);
    QString translate(const char *context, const char *sourceText, const char *disambiguation = nullptr, int n = -1) const override;
    bool isEmpty() const override { return m_map.isEmpty(); }
    int size() const { return int(m_map.size()); }

private:
    QHash<QByteArray, QString> m_map;
};

// The language of the interface: Spanish (the language the program is written in), English, Portuguese, Korean,
// Chinese (simplified) and Japanese. `AppSettings::language` holds the choice ("system" follows Windows); changing it
// swaps the translator and asks QML to evaluate every qsTr() again, so no restart is needed.
class Translations : public QObject {
    Q_OBJECT
    // The code actually in use ("es", "en", "pt", "ko", "zh", "ja").
    Q_PROPERTY(QString active READ active NOTIFY activeChanged)
    // [{code, name}] - `name` is the language's own name; "system" first.
    Q_PROPERTY(QVariantList languages READ languages CONSTANT)
    // Goes up with every change: bindings that read text from C++ (effect names...) depend on it to refresh.
    Q_PROPERTY(int revision READ revision NOTIFY activeChanged)

public:
    Translations(QQmlEngine *engine, AppSettings *settings, QObject *parent = nullptr);

    QString active() const { return m_active; }
    QVariantList languages() const;
    int revision() const { return m_revision; }

    // The supported language for a settings value: "system" -> the one of Windows (English when it is none of ours).
    static QString resolve(const QString &choice, const QLocale &system = QLocale::system());
    static QStringList codes();

    // Installs the translator of `code` (an unknown one counts as Spanish). Returns the code in use.
    QString apply(const QString &code);

signals:
    void activeChanged();

private:
    QQmlEngine *m_engine;
    AppSettings *m_settings;
    CatalogTranslator m_translator;
    QString m_active = QStringLiteral("es");
    int m_revision = 0;
    bool m_installed = false;
};
