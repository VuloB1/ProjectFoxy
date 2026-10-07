#include "Translations.h"

#include "AppSettings.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QQmlEngine>

namespace {

struct Language {
    const char *code;
    const char *name; // its own name, whatever the language in use
};

constexpr Language kLanguages[] = {
    {"es", "Español"}, {"en", "English"}, {"pt", "Português"}, {"ko", "한국어"}, {"zh", "中文（简体）"}, {"ja", "日本語"},
};

} // namespace

bool CatalogTranslator::loadJsonData(const QByteArray &json)
{
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &error);
    m_map.clear();
    if (error.error != QJsonParseError::NoError || !doc.isObject())
        return false;
    const QJsonObject object = doc.object();
    m_map.reserve(object.size());
    for (auto it = object.begin(); it != object.end(); ++it)
        if (it.value().isString() && !it.value().toString().isEmpty())
            m_map.insert(it.key().toUtf8(), it.value().toString());
    return true;
}

bool CatalogTranslator::loadJson(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        m_map.clear();
        return false;
    }
    return loadJsonData(file.readAll());
}

QString CatalogTranslator::translate(const char *, const char *sourceText, const char *, int) const
{
    if (!sourceText)
        return {};
    const auto it = m_map.constFind(QByteArray(sourceText));
    return it == m_map.constEnd() ? QString() : it.value(); // null: Qt keeps the source text
}

QStringList Translations::codes()
{
    QStringList list;
    for (const Language &l : kLanguages)
        list << QString::fromLatin1(l.code);
    return list;
}

QString Translations::resolve(const QString &choice, const QLocale &system)
{
    if (codes().contains(choice))
        return choice;
    // "system": the first of Windows' preferred languages that we have
    for (const QString &name : system.uiLanguages()) {
        const QString lang = name.section(QLatin1Char('-'), 0, 0).toLower();
        if (codes().contains(lang))
            return lang;
    }
    return QStringLiteral("en");
}

Translations::Translations(QQmlEngine *engine, AppSettings *settings, QObject *parent)
    : QObject(parent), m_engine(engine), m_settings(settings)
{
    apply(resolve(settings->language()));
    connect(settings, &AppSettings::languageChanged, this, [this] { apply(resolve(m_settings->language())); });
}

QVariantList Translations::languages() const
{
    QVariantList list;
    list.append(QVariantMap{{"code", "system"}, {"name", QString()}});
    for (const Language &l : kLanguages)
        list.append(QVariantMap{{"code", QString::fromLatin1(l.code)}, {"name", QString::fromUtf8(l.name)}});
    return list;
}

QString Translations::apply(const QString &requested)
{
    const QString code = codes().contains(requested) ? requested : QStringLiteral("es");
    if (m_installed) {
        QCoreApplication::removeTranslator(&m_translator);
        m_installed = false;
    }
    if (code != QLatin1String("es")) { // Spanish is the source text: no catalogue
        m_translator.loadJson(QStringLiteral(":/i18n/%1.json").arg(code));
        QCoreApplication::installTranslator(&m_translator);
        m_installed = true;
    }
    const bool changed = code != m_active;
    m_active = code;
    ++m_revision;
    if (m_engine)
        m_engine->retranslate();
    if (changed || m_revision == 1)
        emit activeChanged();
    return code;
}
