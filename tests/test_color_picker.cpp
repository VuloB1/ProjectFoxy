#include <QtTest>
#include "ColorPicker.h"

// The eyedropper of the colour readout: the colour as text and the shape of its cursor.
class TestColorPicker : public QObject {
    Q_OBJECT
private slots:
    void colourAsText()
    {
        ColorPicker picker;
        QCOMPARE(picker.hex(11, 39, 227), QString("#0B27E3"));
        QCOMPARE(picker.hex(0, 0, 0), QString("#000000"));
        QCOMPARE(picker.hex(255, 255, 255, 255), QString("#FFFFFF"));
        QCOMPARE(picker.hex(255, 128, 0, 64), QString("#FF800040")); // not opaque: the alpha follows
        QCOMPARE(picker.hex(300, -5, 7), QString("#FF0007"));          // kept in range
    }

    void theCursorPointsUpAndToTheRightFromItsTip()
    {
        const QImage img = ColorPicker::eyedropperPixmap().toImage().convertToFormat(QImage::Format_ARGB32);
        QVERIFY(img.width() >= 32 && img.width() <= 48); // about the size of a normal cursor
        if (qEnvironmentVariableIsSet("EYEDROPPER_OUT"))
            img.save(qEnvironmentVariable("EYEDROPPER_OUT"));
        auto opaque = [&](int x, int y) { return x >= 0 && y >= 0 && x < img.width() && y < img.height() && qAlpha(img.pixel(x, y)) > 40; };
        // the tip is at the bottom left (where the hot spot is), the bulb at the top right
        int left = img.width(), right = 0, top = img.height(), bottom = 0;
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x)
                if (opaque(x, y)) {
                    left = std::min(left, x); right = std::max(right, x);
                    top = std::min(top, y); bottom = std::max(bottom, y);
                }
        QVERIFY(left >= 0 && top >= 0 && right < img.width() && bottom < img.height());
        QVERIFY(left < img.width() / 4);                  // the tip is near the left edge...
        QVERIFY(bottom > img.height() * 3 / 4);           // ...and the bottom one
        QVERIFY(right > img.width() / 2 && top < img.height() / 3); // the bulb is up and to the right
        // nothing hangs in the opposite corners
        QVERIFY(!opaque(img.width() - 3, img.height() - 3));
        QVERIFY(!opaque(3, 3));
        // the hot spot (3, 29 on the 32 grid) sits on the drawing
        const double k = img.width() / 32.0;
        bool near = false;
        for (int dy = -3; dy <= 3; ++dy)
            for (int dx = -3; dx <= 3; ++dx)
                near = near || opaque(int(3 * k) + dx, int(29 * k) + dy);
        QVERIFY(near);
    }
};

QTEST_MAIN(TestColorPicker)
#include "test_color_picker.moc"
