#include "TextParagraphs.h"

#include <QtTest>

class TestTextParagraphs : public QObject {
    Q_OBJECT

private slots:
    void closeLinesFormOneParagraph()
    {
        // three lines of 20 px with 4 px between them, then a gap of 40 px and two more
        const std::vector<QRectF> lines{{10, 0, 300, 20}, {10, 24, 280, 20}, {10, 48, 150, 20}, {10, 108, 300, 20}, {10, 132, 120, 20}};
        QCOMPARE(core::paragraphsOf(lines), (std::vector<int>{0, 0, 0, 1, 1}));
    }

    void aHeadingAndItsBodyAreTwoParagraphs()
    {
        // a 40 px title over 16 px lines: the letters differ in size
        const std::vector<QRectF> lines{{10, 0, 400, 40}, {10, 46, 400, 16}, {10, 66, 380, 16}};
        QCOMPARE(core::paragraphsOf(lines), (std::vector<int>{0, 1, 1}));
    }

    void linesSideBySideAreNotJoined()
    {
        const std::vector<QRectF> lines{{10, 0, 100, 20}, {300, 22, 100, 20}};
        QCOMPARE(core::paragraphsOf(lines), (std::vector<int>{0, 1}));
    }

    void emptyInput()
    {
        QVERIFY(core::paragraphsOf({}).empty());
    }
};

QTEST_MAIN(TestTextParagraphs)
#include "test_text_paragraphs.moc"
