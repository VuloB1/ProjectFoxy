#include "ThemeManager.h"
#include <QEasingCurve>
#include <QVariantMap>

ThemeManager::ThemeManager(QObject *parent)
    : QObject(parent)
{
}

const std::array<ThemeManager::ThemeSpec, 4> &ThemeManager::themes()
{
    static const std::array<ThemeSpec, 4> kThemes = { {
        {
            "modern-light", tr("Moderno Claro"), "modern",
            QColor("#f3f4f6"), QColor("#ffffff"), QColor("#eceef1"),
            QColor("#1a1a1a"), QColor("#66707a"), QColor("#dde1e6"),
            QColor("#3d8bfd"), QColor("#ffffff"),
            QColor("#ffffff"),
            QColor("#ffffff"), QColor("#f0f0f0"), QColor("#c0c0c0"), QColor("#808080"),
            4, 8, 120, 220, int(QEasingCurve::OutCubic),
            QStringLiteral("Segoe UI"),
        },
        {
            "modern-dark", tr("Moderno Oscuro"), "modern",
            QColor("#1b1c1f"), QColor("#232427"), QColor("#2b2d31"),
            QColor("#f2f2f2"), QColor("#9a9fa6"), QColor("#3a3c40"),
            QColor("#4cc2ff"), QColor("#0c0c0c"),
            QColor("#2b2d31"),
            QColor("#3a3c40"), QColor("#333333"), QColor("#141414"), QColor("#000000"),
            4, 8, 120, 220, int(QEasingCurve::OutCubic),
            QStringLiteral("Segoe UI"),
        },
        {
            "win98", tr("Classic Claro"), "win98",
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
            "win98-dark", tr("Classic Oscuro"), "win98",
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
        entry[QStringLiteral("accent")] = spec.accent;
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
