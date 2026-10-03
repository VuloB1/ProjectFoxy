#include "Looks.h"

#include <algorithm>

namespace core::edit {

namespace {

using Rgb = std::array<double, 3>;

// 0xRRGGBB -> 0..1 components.
constexpr Rgb rgb(unsigned hex)
{
    return {((hex >> 16) & 0xFF) / 255.0, ((hex >> 8) & 0xFF) / 255.0, (hex & 0xFF) / 255.0};
}

constexpr double luma(const Rgb &c)
{
    return 0.299 * c[0] + 0.587 * c[1] + 0.114 * c[2];
}

// Small fluent builder so the catalogue below reads like a list of recipes.
class Builder {
public:
    Builder(const char *id, const char *name, const char *group)
    {
        m_spec.id = QString::fromUtf8(id);
        m_spec.name = QString::fromUtf8(name);
        m_spec.group = QString::fromUtf8(group);
    }

    // --- tone / color (the same controls as the Ajustes tab) ---
    Builder &exposure(double v) { m_spec.tone.exposure = v; return *this; }
    Builder &brightness(double v) { m_spec.tone.brightness = v; return *this; }
    Builder &contrast(double v) { m_spec.tone.contrast = v; return *this; }
    Builder &highlights(double v) { m_spec.tone.highlights = v; return *this; }
    Builder &shadows(double v) { m_spec.tone.shadows = v; return *this; }
    Builder &blacks(double v) { m_spec.tone.blacks = v; return *this; }
    Builder &temperature(double v) { m_spec.tone.temperature = v; return *this; }
    Builder &tint(double v) { m_spec.tone.tint = v; return *this; }
    Builder &saturation(double v) { m_spec.tone.saturation = v; return *this; }
    Builder &vibrance(double v) { m_spec.tone.vibrance = v; return *this; }
    // Lifts the blacks (and optionally lowers the whites): the faded print look.
    Builder &fade(double black, double white = 1.0)
    {
        m_spec.tone.outBlack = black;
        m_spec.tone.outWhite = white;
        return *this;
    }
    // channel: 0 = all, 1..3 = red, green, blue
    Builder &curve(int channel, CurvePoints pts)
    {
        m_spec.tone.curves[channel] = std::move(pts);
        return *this;
    }

    // --- look-only extras ---
    // Tints the shadows / the highlights with a color at the given strength.
    Builder &splitTone(unsigned shadowColor, double shadowAmount, unsigned highlightColor, double highlightAmount)
    {
        const Rgb s = rgb(shadowColor);
        const Rgb h = rgb(highlightColor);
        for (int i = 0; i < 3; ++i) {
            m_spec.extras.shadowOffset[i] = (s[i] - luma(s)) * shadowAmount;
            m_spec.extras.highlightOffset[i] = (h[i] - luma(h)) * highlightAmount;
        }
        return *this;
    }
    // Replaces the colors with a gradient: dark tones take `dark`, light tones
    // take `light`, the middle takes `mid` (the average of the two if omitted).
    Builder &gradient(unsigned dark, unsigned light, int mid = -1)
    {
        const Rgb d = rgb(dark);
        const Rgb l = rgb(light);
        Rgb m{};
        for (int i = 0; i < 3; ++i)
            m[i] = (d[i] + l[i]) * 0.5;
        if (mid >= 0)
            m = rgb(static_cast<unsigned>(mid));
        m_spec.extras.gradient = true;
        m_spec.extras.stops = {d, m, l};
        return *this;
    }
    Builder &vignette(double v) { m_spec.extras.vignette = v; return *this; }
    Builder &grain(double v) { m_spec.extras.grain = v; return *this; }

    LookSpec build() { return std::move(m_spec); }

private:
    LookSpec m_spec;
};

std::vector<LookSpec> buildCatalogue()
{
    std::vector<LookSpec> v;
    auto add = [&v](Builder b) { v.push_back(b.build()); };

    // --- Cine -------------------------------------------------------------
    add(Builder("cine", "Cine", "cine")
            .contrast(0.10).saturation(0.05)
            .curve(0, {{0, 0}, {0.25, 0.21}, {0.75, 0.80}, {1, 1}})
            .splitTone(0x0d7388, 0.32, 0xff9a40, 0.30));
    add(Builder("blockbuster", "Blockbuster", "cine")
            .contrast(0.20).saturation(0.12).blacks(-0.08)
            .splitTone(0x0d7388, 0.48, 0xff9a40, 0.40)
            .vignette(0.25));
    add(Builder("cine_frio", "Cine frío", "cine")
            .temperature(-0.25).contrast(0.14).saturation(-0.08)
            .splitTone(0x1a40a6, 0.38, 0xccddff, 0.14));
    add(Builder("cine_calido", "Cine cálido", "cine")
            .temperature(0.22).contrast(0.10).fade(0.03)
            .splitTone(0x733f1a, 0.28, 0xffbf59, 0.30));
    add(Builder("matrix", "Matrix", "cine")
            .tint(0.35).saturation(-0.30).contrast(0.20)
            .splitTone(0x0d8c40, 0.48, 0x99ffb3, 0.20));
    add(Builder("amanecer", "Amanecer", "cine")
            .temperature(0.18).brightness(0.03).fade(0.04)
            .splitTone(0xbf3380, 0.30, 0xffc773, 0.35));
    add(Builder("crepusculo", "Crepúsculo", "cine")
            .exposure(-0.18).saturation(-0.12).contrast(0.14)
            .splitTone(0x2633b3, 0.50, 0xcc80e6, 0.20));

    // --- Retro ------------------------------------------------------------
    add(Builder("polaroid", "Polaroid", "retro")
            .fade(0.07).contrast(-0.06).temperature(0.10).saturation(-0.08)
            .splitTone(0x4d664d, 0.15, 0xffedbf, 0.25)
            .vignette(0.22));
    add(Builder("instantanea", "Instantánea", "retro")
            .fade(0.05).saturation(-0.05).contrast(0.05)
            .splitTone(0x338c66, 0.30, 0xffbfd9, 0.22));
    add(Builder("desvaido", "Desvaído", "retro")
            .fade(0.12, 0.96).saturation(-0.35).contrast(-0.10));
    add(Builder("setentas", "Años 70", "retro")
            .temperature(0.28).saturation(0.05).fade(0.05).contrast(-0.05)
            .grain(0.30).vignette(0.30));
    add(Builder("ochentas", "Años 80", "retro")
            .contrast(0.14).saturation(0.20)
            .splitTone(0xb3269a, 0.40, 0x4de6f2, 0.28));
    add(Builder("cruzado", "Proceso cruzado", "retro")
            .curve(1, {{0, 0}, {0.25, 0.14}, {0.75, 0.90}, {1, 1}})
            .curve(3, {{0, 0.10}, {0.5, 0.48}, {1, 0.86}})
            .saturation(0.18).contrast(0.08));
    add(Builder("lomo", "Lomo", "retro")
            .contrast(0.28).saturation(0.32).blacks(-0.10).temperature(0.04)
            .vignette(0.65));
    add(Builder("camara_vieja", "Cámara vieja", "retro")
            .saturation(-0.45).temperature(0.30).fade(0.06).contrast(-0.05)
            .grain(0.40).vignette(0.40));
    add(Builder("vintage", "Vintage", "retro")
            .contrast(-0.10).saturation(-0.30).temperature(0.20).fade(0.03).brightness(-0.02));

    // --- Blanco y negro ----------------------------------------------------
    add(Builder("bw", "Blanco y negro", "bn").saturation(-1.0));
    add(Builder("bn_contraste", "B/N contrastado", "bn").saturation(-1.0).contrast(0.35));
    add(Builder("bn_suave", "B/N suave", "bn").saturation(-1.0).contrast(-0.15).fade(0.05, 0.97));
    add(Builder("bn_pelicula", "B/N película", "bn")
            .saturation(-1.0).contrast(0.20).grain(0.40).vignette(0.20));
    add(Builder("noir", "Noir", "bn")
            .saturation(-1.0).contrast(0.50).blacks(-0.25).exposure(-0.08).vignette(0.50));
    add(Builder("sepia", "Sepia", "bn").gradient(0x1c1209, 0xfff0cc, 0xa17850));
    add(Builder("cianotipo", "Cianotipo", "bn").gradient(0x051a38, 0xf2f7ff, 0x296b9e));

    // --- Duotono -----------------------------------------------------------
    add(Builder("duo_azul_naranja", "Azul y naranja", "duotono").contrast(0.10).gradient(0x141a5c, 0xffb457));
    add(Builder("duo_purpura_oro", "Púrpura y oro", "duotono").contrast(0.10).gradient(0x2b0a4d, 0xffd166));
    add(Builder("duo_rojo_crema", "Rojo y crema", "duotono").contrast(0.10).gradient(0x4a0c14, 0xfff0d6));
    add(Builder("duo_verde_menta", "Verde menta", "duotono").contrast(0.10).gradient(0x06392a, 0xd8ffe8));
    add(Builder("duo_oceano", "Océano", "duotono").contrast(0.10).gradient(0x03203a, 0x86dcff));
    add(Builder("duo_magenta_cian", "Magenta y cian", "duotono").contrast(0.10).gradient(0x4a1163, 0x7ffcf0));
    add(Builder("duo_atardecer", "Atardecer", "duotono").contrast(0.10).gradient(0x2b0f54, 0xff8a5b, 0xc4467a));
    add(Builder("duo_grafito", "Grafito", "duotono").contrast(0.08).gradient(0x111827, 0xe5edf7));

    // --- Color -------------------------------------------------------------
    add(Builder("calido", "Cálido", "color").temperature(0.30).saturation(0.08));
    add(Builder("frio", "Frío", "color").temperature(-0.30).saturation(0.05));
    add(Builder("otono", "Otoño", "color")
            .temperature(0.28).saturation(0.18).contrast(0.10)
            .splitTone(0x663814, 0.25, 0xffb34d, 0.30));
    add(Builder("verano", "Verano", "color")
            .vibrance(0.35).temperature(0.14).brightness(0.03).contrast(0.08));
    add(Builder("dramatico", "Dramático", "color")
            .contrast(0.35).highlights(-0.30).shadows(0.25).saturation(-0.10).vignette(0.25));
    add(Builder("pastel", "Pastel", "color")
            .fade(0.08).saturation(-0.15).brightness(0.06).contrast(-0.15));
    add(Builder("neon", "Neón", "color")
            .saturation(0.55).vibrance(0.30).contrast(0.25)
            .splitTone(0x661acc, 0.30, 0x33e6ff, 0.20));
    add(Builder("vivid", "Vívido", "color").contrast(0.15).saturation(0.40).vibrance(0.20));

    return v;
}

} // namespace

const std::vector<LookGroup> &lookGroups()
{
    static const std::vector<LookGroup> groups = {
        {QStringLiteral("cine"), QString::fromUtf8("Cine")},
        {QStringLiteral("retro"), QString::fromUtf8("Retro")},
        {QStringLiteral("bn"), QString::fromUtf8("B/N")},
        {QStringLiteral("duotono"), QString::fromUtf8("Duotono")},
        {QStringLiteral("color"), QString::fromUtf8("Color")},
    };
    return groups;
}

const std::vector<LookSpec> &allLooks()
{
    static const std::vector<LookSpec> looks = buildCatalogue();
    return looks;
}

const LookSpec *findLook(const QString &id)
{
    if (id.isEmpty())
        return nullptr;
    for (const LookSpec &look : allLooks()) {
        if (look.id == id)
            return &look;
    }
    return nullptr;
}

QImage applyLook(const QImage &source, const QString &id, double amount)
{
    const LookSpec *look = findLook(id);
    if (!look)
        return source;
    return applyGrade(source, look->tone, look->extras, amount);
}

} // namespace core::edit
