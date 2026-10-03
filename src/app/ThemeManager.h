#pragma once

#include <QObject>
#include <QColor>
#include <QString>
#include <QVariantList>
#include <array>

// Owns every design token the UI reads: colors, corner radii, animation
// timings/easing, and font family, one row per selectable theme ("Moderno
// Claro/Oscuro", "Windows 98"). These live on this plain QObject (registered
// as the "themeManager" context property in main.cpp) rather than on a QML
// "pragma Singleton" object, because this project already hit a real bug
// where a singleton QtObject's property changes don't propagate to bindings
// in OTHER separately-compiled QML files under this qmlcachegen setup -
// anything that needs to change at runtime (every one of these tokens, now
// that theme switching exists) has to live on a plain QObject exposed as a
// context property instead, exactly like AppController itself already is.
class ThemeManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString themeId READ themeId NOTIFY themeChanged)
    // "modern" or "win98" - which structural rendering branch each themed
    // QML control (qml/controls/App*.qml) should use. Not a Q_ENUM: this
    // codebase already prefers plain string ids (presetId, requestId) over
    // enums for this kind of QML-facing branch selector.
    Q_PROPERTY(QString skin READ skin NOTIFY themeChanged)
    // [{id, name}, ...] for the "Tema" menu - fixed for the process lifetime.
    Q_PROPERTY(QVariantList availableThemes READ availableThemes CONSTANT)

    Q_PROPERTY(QColor background READ background NOTIFY themeChanged)
    Q_PROPERTY(QColor surface READ surface NOTIFY themeChanged)
    Q_PROPERTY(QColor surfaceElevated READ surfaceElevated NOTIFY themeChanged)
    Q_PROPERTY(QColor textPrimary READ textPrimary NOTIFY themeChanged)
    Q_PROPERTY(QColor textSecondary READ textSecondary NOTIFY themeChanged)
    Q_PROPERTY(QColor border READ border NOTIFY themeChanged)
    Q_PROPERTY(QColor accent READ accent NOTIFY themeChanged)
    // Text/glyph color legible on top of a filled `accent` area (e.g. a
    // checked checkbox, a selected menu row).
    Q_PROPERTY(QColor accentText READ accentText NOTIFY themeChanged)
    // Base fill for an interactive control's face - distinct from a static
    // panel's `surface` so a future theme can distinguish them even where
    // the flat Modern themes currently set both equal.
    Q_PROPERTY(QColor controlFace READ controlFace NOTIFY themeChanged)
    Q_PROPERTY(QColor fieldFace READ fieldFace NOTIFY themeChanged)
    // Win98-only: the 4 tones of a classic chiseled 3D bevel (outer white/
    // black + inner light-gray/dark-gray). Unused (but present) elsewhere.
    Q_PROPERTY(QColor bevelLight READ bevelLight NOTIFY themeChanged)
    Q_PROPERTY(QColor bevelLightSoft READ bevelLightSoft NOTIFY themeChanged)
    Q_PROPERTY(QColor bevelDark READ bevelDark NOTIFY themeChanged)
    Q_PROPERTY(QColor bevelDarkest READ bevelDarkest NOTIFY themeChanged)

    Q_PROPERTY(int radiusSmall READ radiusSmall NOTIFY themeChanged)
    Q_PROPERTY(int radiusMedium READ radiusMedium NOTIFY themeChanged)
    Q_PROPERTY(int animFast READ animFast NOTIFY themeChanged)
    Q_PROPERTY(int animMedium READ animMedium NOTIFY themeChanged)
    // A QEasingCurve::Type value - QML's Easing enum mirrors it 1:1, so this
    // assigns straight to an `easing.type` binding on the QML side.
    Q_PROPERTY(int easingCurve READ easingCurve NOTIFY themeChanged)
    Q_PROPERTY(QString fontFamily READ fontFamily NOTIFY themeChanged)

public:
    explicit ThemeManager(QObject *parent = nullptr);

    QString themeId() const { return current().id; }
    QString skin() const { return current().skin; }
    QVariantList availableThemes() const;

    QColor background() const { return current().background; }
    QColor surface() const { return current().surface; }
    QColor surfaceElevated() const { return current().surfaceElevated; }
    QColor textPrimary() const { return current().textPrimary; }
    QColor textSecondary() const { return current().textSecondary; }
    QColor border() const { return current().border; }
    QColor accent() const { return current().accent; }
    QColor accentText() const { return current().accentText; }
    QColor controlFace() const { return current().controlFace; }
    QColor fieldFace() const;
    QColor bevelLight() const { return current().bevelLight; }
    QColor bevelLightSoft() const { return current().bevelLightSoft; }
    QColor bevelDark() const { return current().bevelDark; }
    QColor bevelDarkest() const { return current().bevelDarkest; }

    int radiusSmall() const { return current().radiusSmall; }
    int radiusMedium() const { return current().radiusMedium; }
    int animFast() const { return current().animFast; }
    int animMedium() const { return current().animMedium; }
    int easingCurve() const { return current().easingCurve; }
    QString fontFamily() const { return current().fontFamily; }

public slots:
    void setTheme(const QString &id);

signals:
    // One umbrella signal for every property above, same pattern the old
    // darkModeChanged used to fan out to 6 properties - just generalized.
    void themeChanged();

private:
    struct ThemeSpec {
        QString id;
        QString displayName;
        QString skin;

        QColor background, surface, surfaceElevated;
        QColor textPrimary, textSecondary, border;
        QColor accent, accentText;
        QColor controlFace;
        QColor bevelLight, bevelLightSoft, bevelDark, bevelDarkest;

        int radiusSmall, radiusMedium;
        int animFast, animMedium;
        int easingCurve;
        QString fontFamily;
    };

    // Function-local static (not a class-static initialized before main()):
    // avoids any static-initialization-order question around QColor/QString
    // construction, and tr() below is only ever called after QGuiApplication
    // exists, since the first ThemeManager is constructed in main.cpp.
    static const std::array<ThemeSpec, 4> &themes();
    const ThemeSpec &current() const { return themes()[m_currentIndex]; }

    int m_currentIndex = 1; // modern-dark, matching the previous m_darkMode default of true
};
