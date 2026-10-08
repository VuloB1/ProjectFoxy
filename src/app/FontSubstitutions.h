#pragma once

#include <QFont>
#include <QStringList>

// The program names Windows fonts (Segoe UI for the interface, Arial / Impact / Georgia... for captions, Tahoma for
// the Classic skin, Consolas for hex codes) and saves those names in its settings and recipes, so the names stay.
// Where such a font is not installed (Linux, mostly) Qt looks the family up in these lists, in order, and only
// when the family itself is missing - so on Windows nothing changes. The first of each list that exists is used;
// the last entry of each is a generic family fontconfig always resolves.
inline void installFontSubstitutions()
{
#ifndef Q_OS_WIN
    struct Entry { const char *wanted; QStringList instead; };
    const Entry table[] = {
        {"Segoe UI", {"Noto Sans", "Cantarell", "Ubuntu", "Open Sans", "DejaVu Sans", "Liberation Sans", "sans-serif"}},
        {"Tahoma", {"DejaVu Sans", "Noto Sans", "Liberation Sans", "sans-serif"}},
        {"Arial", {"Liberation Sans", "Arimo", "Helvetica", "Nimbus Sans", "DejaVu Sans", "sans-serif"}},
        {"Impact", {"Anton", "Oswald", "Bebas Neue", "Liberation Sans Narrow", "DejaVu Sans", "sans-serif"}},
        {"Georgia", {"Gelasio", "Noto Serif", "DejaVu Serif", "Liberation Serif", "serif"}},
        {"Times New Roman", {"Liberation Serif", "Tinos", "Nimbus Roman", "DejaVu Serif", "serif"}},
        {"Comic Sans MS", {"Comic Neue", "Comic Relief", "Noto Sans", "DejaVu Sans", "sans-serif"}},
        {"Courier New", {"Liberation Mono", "Cousine", "Nimbus Mono PS", "DejaVu Sans Mono", "monospace"}},
        {"Consolas", {"DejaVu Sans Mono", "Liberation Mono", "Noto Sans Mono", "monospace"}},
    };
    for (const Entry &e : table)
        QFont::insertSubstitutions(QString::fromLatin1(e.wanted), e.instead);
#endif
}
