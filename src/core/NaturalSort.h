#pragma once

#include <QString>

namespace core {

// Natural order, the one people expect from file names: "page2" before "page10", case ignored. Written out instead of
// QCollator::setNumericMode because a Qt built without ICU (the Linux builds) silently ignores that mode.
// Returns < 0, 0 or > 0 like strcmp.
inline int naturalCompare(const QString &a, const QString &b)
{
    qsizetype i = 0, j = 0;
    int zerosTie = 0; // "a01" and "a1" are the same number: the shorter spelling goes first, but only as a last resort
    while (i < a.size() && j < b.size()) {
        if (a[i].isDigit() && b[j].isDigit()) {
            qsizetype ei = i, ej = j;
            while (ei < a.size() && a[ei].isDigit())
                ++ei;
            while (ej < b.size() && b[ej].isDigit())
                ++ej;
            qsizetype si = i, sj = j; // without the leading zeros
            while (si < ei - 1 && a[si] == QLatin1Char('0'))
                ++si;
            while (sj < ej - 1 && b[sj] == QLatin1Char('0'))
                ++sj;
            if (ei - si != ej - sj)
                return (ei - si) < (ej - sj) ? -1 : 1; // fewer digits, smaller number
            for (qsizetype k = 0; k < ei - si; ++k)
                if (a[si + k] != b[sj + k])
                    return a[si + k] < b[sj + k] ? -1 : 1;
            if (!zerosTie && (ei - i) != (ej - j))
                zerosTie = (ei - i) < (ej - j) ? -1 : 1;
            i = ei;
            j = ej;
            continue;
        }
        const QChar ca = a[i].toCaseFolded(), cb = b[j].toCaseFolded();
        if (ca != cb)
            return ca < cb ? -1 : 1;
        ++i;
        ++j;
    }
    if (i < a.size() || j < b.size())
        return i < a.size() ? 1 : -1; // the shorter one first
    return zerosTie;
}

} // namespace core
