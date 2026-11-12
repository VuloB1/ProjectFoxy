#include "ThemeManager.h"
#include "AppSettings.h"
#include <QEasingCurve>
#include <QVariantMap>
#include <cmath>

namespace {
// The accent swatches: {id, colour on the light theme, colour on the dark one}. The first is the
// default, the orange of the logo; the dark column is a little lighter so it still glows on charcoal.
struct Accent { const char *id, *light, *dark; };
constexpr Accent kAccents[] = {
    { "orange", "#f06a0f", "#ff8126" },
    { "blue",   "#3d8bfd", "#4cc2ff" },
    { "teal",   "#0d9aa8", "#2dd4bf" },
    { "green",  "#2f9e5b", "#4ade80" },
    { "violet", "#7c5cf0", "#a78bfa" },
    { "pink",   "#e0488a", "#f472b6" },
    { "red",    "#d93a3f", "#ff6b6b" },
};

const Accent &accentFor(const QString &id)
{
    for (const Accent &a : kAccents)
        if (id == QLatin1String(a.id))
            return a;
    return kAccents[0];
}

double luminance(const QColor &c)
{
    auto lin = [](double v) { return v <= 0.03928 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4); };
    return 0.2126 * lin(c.redF()) + 0.7152 * lin(c.greenF()) + 0.0722 * lin(c.blueF());
}
}

ThemeManager::ThemeManager(AppSettings *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
{
    connect(settings, &AppSettings::accentIdChanged, this, &ThemeManager::themeChanged);
}

QColor ThemeManager::accentOf(const ThemeSpec &spec) const
{
    if (spec.skin == QLatin1String("win98"))
        return spec.accent;
    const Accent &a = accentFor(m_settings->accentId());
    return QColor(spec.id == QLatin1String("modern-dark") ? a.dark : a.light);
}

QColor ThemeManager::accentText() const
{
    if (current().skin == QLatin1String("win98"))
        return current().accentText;
    // Whichever of white / near-black reads better on the accent.
    return luminance(accent()) > 0.18 ? QColor("#1a1a1a") : QColor("#ffffff");
}

QString ThemeManager::accentId() const
{
    return QString::fromLatin1(accentFor(m_settings->accentId()).id);
}

QVariantList ThemeManager::accentChoices() const
{
    const bool dark = current().id == QLatin1String("modern-dark");
    QVariantList list;
    for (const Accent &a : kAccents) {
        QVariantMap m;
        m[QStringLiteral("id")] = QString::fromLatin1(a.id);
        m[QStringLiteral("color")] = QColor(dark ? a.dark : a.light);
        list.append(m);
    }
    return list;
}

void ThemeManager::setAccent(const QString &id)
{
    m_settings->setAccentId(id);
}

const std::array<ThemeManager::ThemeSpec, 4> &ThemeManager::themes()
{
    static const std::array<ThemeSpec, 4> kThemes = { {
        {
            "modern-light", QT_TR_NOOP("Moderno Claro"), "modern",
            QColor("#f3f4f6"), QColor("#ffffff"), QColor("#eceef1"),
            QColor("#1a1a1a"), QColor("#66707a"), QColor("#dde1e6"),
            QColor("#3d8bfd"), QColor("#ffffff"),
            QColor("#ffffff"),
            QColor("#ffffff"), QColor("#f0f0f0"), QColor("#c0c0c0"), QColor("#808080"),
            4, 8, 120, 220, int(QEasingCurve::OutCubic),
            QStringLiteral("Segoe UI"),
        },
        {
            "modern-dark", QT_TR_NOOP("Moderno Oscuro"), "modern",
            QColor("#1b1c1f"), QColor("#232427"), QColor("#2b2d31"),
            QColor("#f2f2f2"), QColor("#9a9fa6"), QColor("#3a3c40"),
            QColor("#4cc2ff"), QColor("#0c0c0c"),
            QColor("#2b2d31"),
            QColor("#3a3c40"), QColor("#333333"), QColor("#141414"), QColor("#000000"),
            4, 8, 120, 220, int(QEasingCurve::OutCubic),
            QStringLiteral("Segoe UI"),
        },
        {
            "win98", QT_TR_NOOP("Classic Claro"), "win98",
            QColor("#808080"), QColor("#c0c0c0"), QColor("#c0c0c0"),
            QColor("#000000"), QColor("#000000"), QColor("#000000"),
            QColor("#000080"), QColor("#ffffff"),
            QColor("#c0c0c0"),
            QColor("#ffffff"), QColor("#dfdfdf"), QColor("#808080"), QColor("#000000"),
            0, 0, 0, 0, int(QEasingCurve::Linear),
            // MS Sans Serif (the "authentic" pick) is a legacy bitmap font
            // that doesn't cover accented characters or the "…" ellipsis
            // used in this UI's Spanish strings - they render as garbled
            // boxes. Tahoma is still period-correct (Win98 SE/2000-era)
            // and a real TrueType font with full coverage.
            QStringLiteral("Tahoma"),
        },
        {
            // The same chiselled look in a dark palette: charcoal faces, light text,
            // bevels turned around (light edge = mid grey, shadow = near black).
            "win98-dark", QT_TR_NOOP("Classic Oscuro"), "win98",
            QColor("#1c1c1c"), QColor("#3a3a3a"), QColor("#3a3a3a"),
            QColor("#f0f0f0"), QColor("#f0f0f0"), QColor("#000000"),
            QColor("#2a52a8"), QColor("#ffffff"),
            QColor("#3a3a3a"),
            QColor("#8a8a8a"), QColor("#5c5c5c"), QColor("#242424"), QColor("#000000"),
            0, 0, 0, 0, int(QEasingCurve::Linear),
            QStringLiteral("Tahoma"),
        },
    } };
    return kThemes;
}

QVariantList ThemeManager::availableThemes() const
{
    QVariantList list;
    list.reserve(int(themes().size()));
    for (const auto &spec : themes()) {
        QVariantMap entry;
        entry[QStringLiteral("id")] = spec.id;
        entry[QStringLiteral("name")] = spec.displayName;
        entry[QStringLiteral("skin")] = spec.skin;
        // The palette, so the settings page can draw a preview card per theme.
        entry[QStringLiteral("background")] = spec.background;
        entry[QStringLiteral("surface")] = spec.surface;
        entry[QStringLiteral("surfaceElevated")] = spec.surfaceElevated;
        entry[QStringLiteral("textPrimary")] = spec.textPrimary;
        entry[QStringLiteral("textSecondary")] = spec.textSecondary;
        entry[QStringLiteral("border")] = spec.border;
        entry[QStringLiteral("accent")] = accentOf(spec);
        list.append(entry);
    }
    return list;
}

void ThemeManager::setTheme(const QString &id)
{
    const auto &specs = themes();
    for (int i = 0; i < int(specs.size()); ++i) {
        if (specs[i].id == id) {
            if (i != m_currentIndex) {
                m_currentIndex = i;
                emit themeChanged();
            }
            return;
        }
    }
}

QColor ThemeManager::fieldFace() const
{
    // The sunken text wells of the Win98 skin: white, or near black in its dark variant.
    return current().id == QLatin1String("win98-dark") ? QColor("#141414") : QColor("#ffffff");
}
