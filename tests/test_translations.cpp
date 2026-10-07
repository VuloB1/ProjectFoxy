#include <QtTest>
#include "Translate.h"
#include "Translations.h"

// The language of the interface: which one a Windows language maps to, and that every catalogue is complete.
class TestTranslations : public QObject {
    Q_OBJECT
private slots:
    void theChoiceBecomesALanguage()
    {
        QCOMPARE(Translations::resolve(QStringLiteral("ja")), QStringLiteral("ja"));
        QCOMPARE(Translations::resolve(QStringLiteral("es")), QStringLiteral("es"));
        QCOMPARE(Translations::resolve(QStringLiteral("system"), QLocale(QLocale::Portuguese, QLocale::Brazil)), QStringLiteral("pt"));
        QCOMPARE(Translations::resolve(QStringLiteral("system"), QLocale(QLocale::Chinese, QLocale::China)), QStringLiteral("zh"));
        QCOMPARE(Translations::resolve(QStringLiteral("system"), QLocale(QLocale::Spanish, QLocale::Argentina)), QStringLiteral("es"));
        // a language we do not have: English
        QCOMPARE(Translations::resolve(QStringLiteral("system"), QLocale(QLocale::French, QLocale::France)), QStringLiteral("en"));
        QCOMPARE(Translations::codes().size(), 6);
    }

    void aTranslatorLooksTheSpanishTextUp()
    {
        CatalogTranslator tr;
        QVERIFY(tr.loadJsonData(R"({"Cancelar":"Cancel","Hola %1":"Hello %1"})"));
        QCOMPARE(tr.translate("any", "Cancelar"), QStringLiteral("Cancel"));
        QVERIFY(tr.translate("any", "Algo sin traducir").isNull()); // Qt then keeps the source text
        QVERIFY(!tr.loadJsonData("not json"));
        QVERIFY(tr.isEmpty());
    }

    void coreTextGoesThroughTheInstalledTranslator()
    {
        QCOMPARE(core::tr("Cancelar"), QStringLiteral("Cancelar")); // no translator: the Spanish itself
        CatalogTranslator translator;
        QVERIFY(translator.loadJsonData(R"({"Cancelar":"Cancel"})"));
        QCoreApplication::installTranslator(&translator);
        QCOMPARE(core::tr("Cancelar"), QStringLiteral("Cancel"));
        QCOMPARE(core::tr(QStringLiteral("Cancelar")), QStringLiteral("Cancel"));
        QCOMPARE(core::tr("Sin entrada"), QStringLiteral("Sin entrada"));
        QCoreApplication::removeTranslator(&translator);
    }

    void everyCatalogueIsComplete_data()
    {
        QTest::addColumn<QString>("code");
        for (const QString &code : Translations::codes())
            if (code != QLatin1String("es"))
                QTest::newRow(qPrintable(code)) << code;
    }
    void everyCatalogueIsComplete()
    {
        QFETCH(QString, code);
        CatalogTranslator translator;
        QVERIFY(translator.loadJson(QStringLiteral(I18N_DIR "/%1.json").arg(code)));
        QVERIFY2(translator.size() > 650, qPrintable(QString::number(translator.size())));
        // a few texts from every corner of the program, and the placeholders must survive
        for (const char *text : {"Cerrar", "Guardar como…", "Quitar todas", "Brillo", "Radio", "Rejilla",
                                 "No se pudo guardar «%1»: %2", "Leyenda (meme)"}) {
            const QString out = translator.translate("x", text);
            QVERIFY2(!out.isEmpty() && out != QString::fromUtf8(text), text);
            QCOMPARE(out.count(QLatin1Char('%')), QString::fromUtf8(text).count(QLatin1Char('%')));
        }
    }
};

QTEST_MAIN(TestTranslations)
#include "test_translations.moc"
