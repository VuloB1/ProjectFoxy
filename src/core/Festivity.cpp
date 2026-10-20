#include "Festivity.h"

namespace core {

namespace {
struct Name {
    Festival festival;
    const char *key;
};
constexpr Name kNames[] = {
    {Festival::Halloween, "halloween"},   {Festival::DayOfTheDead, "muertos"}, {Festival::Christmas, "navidad"},
    {Festival::NewYear, "nuevoano"},      {Festival::ThreeKings, "reyes"},     {Festival::Valentine, "sanvalentin"},
    {Festival::StPatrick, "sanpatricio"}, {Festival::Easter, "pascua"},
};
} // namespace

QDate easterSunday(int year)
{
    // The anonymous Gregorian algorithm (Meeus/Jones/Butcher).
    const int a = year % 19, b = year / 100, c = year % 100;
    const int d = b / 4, e = b % 4, f = (b + 8) / 25, g = (b - f + 1) / 3;
    const int h = (19 * a + b - d - g + 15) % 30;
    const int i = c / 4, k = c % 4;
    const int l = (32 + 2 * e + 2 * i - h - k) % 7;
    const int m = (a + 11 * h + 22 * l) / 451;
    const int month = (h + l - 7 * m + 114) / 31;
    const int day = (h + l - 7 * m + 114) % 31 + 1;
    return QDate(year, month, day);
}

Festival festivalOn(const QDate &date)
{
    if (!date.isValid())
        return Festival::None;
    const int month = date.month(), day = date.day();

    const QDate easter = easterSunday(date.year());
    if (date >= easter.addDays(-7) && date <= easter.addDays(1))
        return Festival::Easter;

    switch (month) {
    case 1:
        return day <= 2 ? Festival::NewYear : day <= 6 ? Festival::ThreeKings : Festival::None;
    case 2:
        return day >= 7 && day <= 15 ? Festival::Valentine : Festival::None;
    case 3:
        return day >= 12 && day <= 17 ? Festival::StPatrick : Festival::None;
    case 10:
        return Festival::Halloween;
    case 11:
        return day <= 3 ? Festival::DayOfTheDead : Festival::None;
    case 12:
        return day <= 26 ? Festival::Christmas : Festival::NewYear;
    default:
        return Festival::None;
    }
}

QString festivalKey(Festival festival)
{
    for (const Name &n : kNames)
        if (n.festival == festival)
            return QLatin1String(n.key);
    return {};
}

Festival festivalFromKey(const QString &key)
{
    for (const Name &n : kNames)
        if (key == QLatin1String(n.key))
            return n.festival;
    return Festival::None;
}

} // namespace core
