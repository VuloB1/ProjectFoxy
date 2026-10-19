#include "Festivity.h"

#include <QtTest>

using namespace core;

class TestFestivity : public QObject {
    Q_OBJECT

private slots:
    void easterIsComputedForKnownYears()
    {
        QCOMPARE(easterSunday(2024), QDate(2024, 3, 31));
        QCOMPARE(easterSunday(2025), QDate(2025, 4, 20));
        QCOMPARE(easterSunday(2026), QDate(2026, 4, 5));
        QCOMPARE(easterSunday(2027), QDate(2027, 3, 28));
        QCOMPARE(easterSunday(2038), QDate(2038, 4, 25)); // the latest possible
        QCOMPARE(easterSunday(2285), QDate(2285, 3, 22)); // the earliest possible
    }

    void fixedHolidaysCoverTheirRanges()
    {
        QCOMPARE(festivalOn(QDate(2026, 10, 1)), Festival::Halloween);
        QCOMPARE(festivalOn(QDate(2026, 10, 31)), Festival::Halloween);
        QCOMPARE(festivalOn(QDate(2026, 11, 1)), Festival::DayOfTheDead);
        QCOMPARE(festivalOn(QDate(2026, 11, 3)), Festival::DayOfTheDead);
        QCOMPARE(festivalOn(QDate(2026, 11, 4)), Festival::None);
        QCOMPARE(festivalOn(QDate(2026, 11, 30)), Festival::None);
        QCOMPARE(festivalOn(QDate(2026, 12, 1)), Festival::Christmas);
        QCOMPARE(festivalOn(QDate(2026, 12, 26)), Festival::Christmas);
        QCOMPARE(festivalOn(QDate(2026, 12, 27)), Festival::NewYear);
        QCOMPARE(festivalOn(QDate(2026, 12, 31)), Festival::NewYear);
        QCOMPARE(festivalOn(QDate(2027, 1, 1)), Festival::NewYear);
        QCOMPARE(festivalOn(QDate(2027, 1, 2)), Festival::NewYear);
        QCOMPARE(festivalOn(QDate(2027, 1, 3)), Festival::ThreeKings);
        QCOMPARE(festivalOn(QDate(2027, 1, 6)), Festival::ThreeKings);
        QCOMPARE(festivalOn(QDate(2027, 1, 7)), Festival::None);
        QCOMPARE(festivalOn(QDate(2027, 2, 6)), Festival::None);
        QCOMPARE(festivalOn(QDate(2027, 2, 7)), Festival::Valentine);
        QCOMPARE(festivalOn(QDate(2027, 2, 15)), Festival::Valentine);
        QCOMPARE(festivalOn(QDate(2027, 2, 16)), Festival::None);
        QCOMPARE(festivalOn(QDate(2026, 3, 11)), Festival::None);
        QCOMPARE(festivalOn(QDate(2026, 3, 12)), Festival::StPatrick);
        QCOMPARE(festivalOn(QDate(2026, 3, 17)), Festival::StPatrick);
        QCOMPARE(festivalOn(QDate(2026, 3, 18)), Festival::None);
        QCOMPARE(festivalOn(QDate(2026, 7, 15)), Festival::None);
    }

    void easterWeekRunsFromSevenDaysBeforeToTheNextMonday()
    {
        // Easter Sunday 2026 is 5 April.
        QCOMPARE(festivalOn(QDate(2026, 3, 28)), Festival::None);
        QCOMPARE(festivalOn(QDate(2026, 3, 29)), Festival::Easter); // 7 days before
        QCOMPARE(festivalOn(QDate(2026, 4, 5)), Festival::Easter);
        QCOMPARE(festivalOn(QDate(2026, 4, 6)), Festival::Easter); // the Monday after
        QCOMPARE(festivalOn(QDate(2026, 4, 7)), Festival::None);
    }

    void easterBeatsStPatrickWhenTheyOverlap()
    {
        // Easter 2027 is 28 March: its week starts 21 March, after San Patricio (12-17). Easter 2285 is 22 March (starts
        // 15 March): the overlap is 15-17 March and Easter wins.
        QCOMPARE(festivalOn(QDate(2285, 3, 14)), Festival::StPatrick);
        QCOMPARE(festivalOn(QDate(2285, 3, 15)), Festival::Easter);
        QCOMPARE(festivalOn(QDate(2285, 3, 17)), Festival::Easter);
    }

    void keysRoundTrip()
    {
        for (Festival f : {Festival::Halloween, Festival::DayOfTheDead, Festival::Christmas, Festival::NewYear,
                           Festival::ThreeKings, Festival::Valentine, Festival::StPatrick, Festival::Easter}) {
            QVERIFY(!festivalKey(f).isEmpty());
            QCOMPARE(festivalFromKey(festivalKey(f)), f);
        }
        QVERIFY(festivalKey(Festival::None).isEmpty());
        QCOMPARE(festivalFromKey(QStringLiteral("nope")), Festival::None);
        QCOMPARE(festivalOn(QDate()), Festival::None);
    }
};

QTEST_MAIN(TestFestivity)
#include "test_festivity.moc"
