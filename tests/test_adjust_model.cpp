#include "AdjustModel.h"
#include "LookModel.h"
#include <QtTest>

using core::edit::AdjustOp;

namespace {
QVariantMap point(double x, double y)
{
    return {{QStringLiteral("x"), x}, {QStringLiteral("y"), y}};
}
} // namespace

// The Ajustes and Filtros vocabulary, with nothing around it: each request is a function from the current state to
// the next one (or to "nothing changes").
class TestAdjustModel : public QObject {
    Q_OBJECT

private slots:
    // ---- the map QML reads
    void theMapCarriesEverySliderLevelsAndCurves()
    {
        AdjustOp op;
        op.exposure = 0.25;
        op.grainType = 2;
        op.grainMono = true;
        op.negative = true;
        op.outBlack = 0.1;
        op.curves[1] = {QPointF(0.2, 0.3), QPointF(0.7, 0.6)};
        const QVariantMap map = AdjustModel::toMap(op);
        for (const char *key : {"exposure", "brightness", "contrast", "highlights", "shadows", "whites", "blacks", "gamma",
                                "temperature", "tint", "saturation", "vibrance", "hue", "clarity", "sharpness", "vignette",
                                "grain", "grainType", "grainMono", "negative", "outBlack", "outWhite", "levels", "curves"})
            QVERIFY2(map.contains(QLatin1String(key)), key);
        QCOMPARE(map.value("exposure").toDouble(), 0.25);
        QCOMPARE(map.value("grainType").toInt(), 2);
        QVERIFY(map.value("grainMono").toBool());
        QVERIFY(map.value("negative").toBool());
        QCOMPARE(map.value("levels").toList().size(), 4);
        const QVariantList curves = map.value("curves").toList();
        QCOMPARE(curves.size(), 4);                       // one list per channel, not all points in one
        QCOMPARE(curves.at(0).toList().size(), 0);
        QCOMPARE(curves.at(1).toList().size(), 2);
        QCOMPARE(curves.at(1).toList().at(1).toMap().value("y").toDouble(), 0.6);
    }

    // ---- sliders
    void aSliderIsClampedToItsRangeAndKeysItsMerge()
    {
        const AdjustOp neutral;
        auto change = AdjustModel::setParam(neutral, QStringLiteral("exposure"), 5);
        QVERIFY(change);
        QCOMPARE(change->op.exposure, 1.0);
        QCOMPARE(change->key, QStringLiteral("p:exposure"));
        QCOMPARE(AdjustModel::setParam(neutral, QStringLiteral("exposure"), -5)->op.exposure, -1.0);
        QVERIFY(!AdjustModel::setParam(neutral, QStringLiteral("sharpness"), -0.5)); // clamps to the 0 it already has
        QCOMPARE(AdjustModel::setParam(neutral, QStringLiteral("grain"), 3)->op.grain, 1.0);
    }

    void unknownOrUnchangedRequestsChangeNothing()
    {
        AdjustOp op;
        op.contrast = 0.4;
        QVERIFY(!AdjustModel::setParam(op, QStringLiteral("no-such"), 0.5));
        QVERIFY(!AdjustModel::setParam(op, QStringLiteral("contrast"), 0.4));    // same value
        QVERIFY(AdjustModel::setParam(op, QStringLiteral("contrast"), 0.41));
    }

    void aRequestNeverTouchesTheOtherFields()
    {
        AdjustOp op;
        op.saturation = 0.3;
        op.levels[2].inBlack = 0.2;
        const auto change = AdjustModel::setParam(op, QStringLiteral("exposure"), 0.5);
        QCOMPARE(change->op.saturation, 0.3);
        QCOMPARE(change->op.levels[2].inBlack, 0.2);
        QCOMPARE(op.exposure, 0.0); // and the argument itself is not modified
    }

    void grainTypeClampsToItsRange()
    {
        AdjustOp op;
        op.grainType = 1;
        QCOMPARE(AdjustModel::setGrainType(op, 9)->op.grainType, 3);
        QCOMPARE(AdjustModel::setGrainType(op, -2)->op.grainType, 0);
        QVERIFY(!AdjustModel::setGrainType(op, 1));
        QCOMPARE(AdjustModel::setGrainType(op, 2)->key, QStringLiteral("grainType"));
        QVERIFY(AdjustModel::setGrainMono(op, true)->op.grainMono);
        QVERIFY(!AdjustModel::setGrainMono(op, false)); // already so
        QVERIFY(AdjustModel::setNegative(op, true)->op.negative);
        QVERIFY(!AdjustModel::setNegative(op, false));
    }

    // ---- levels
    void levelsKeepBlackBelowWhiteAndGammaInRange()
    {
        const AdjustOp neutral;
        const auto change = AdjustModel::setLevels(neutral, 0, 0.5, 1.0, 0.4); // white below black
        QVERIFY(change);
        QCOMPARE(change->op.levels[0].inBlack, 0.5);
        QVERIFY(change->op.levels[0].inWhite >= 0.52 - 1e-12);
        QCOMPARE(change->key, QStringLiteral("levels:0"));
        QCOMPARE(AdjustModel::setLevels(neutral, 2, 0.0, 50.0, 1.0)->op.levels[2].gamma, 9.99);
        QCOMPARE(AdjustModel::setLevels(neutral, 2, 0.0, 0.0, 1.0)->op.levels[2].gamma, 0.1);
        QCOMPARE(AdjustModel::setLevels(neutral, 1, 2.0, 1.0, 1.0)->op.levels[1].inBlack, 0.98);
        QVERIFY(!AdjustModel::setLevels(neutral, 4, 0.1, 1.0, 0.9));  // no such channel
        QVERIFY(!AdjustModel::setLevels(neutral, -1, 0.1, 1.0, 0.9));
        QVERIFY(!AdjustModel::setLevels(neutral, 0, 0.0, 1.0, 1.0));  // neutral already
    }

    void outputLevelsAndTheirReset()
    {
        const AdjustOp neutral;
        const auto change = AdjustModel::setOutputLevels(neutral, 0.9, 0.2);
        QCOMPARE(change->op.outBlack, 0.9);
        QVERIFY(change->op.outWhite >= 0.92 - 1e-12);
        QCOMPARE(change->key, QStringLiteral("outputLevels"));
        QVERIFY(!AdjustModel::setOutputLevels(neutral, 0.0, 1.0));

        AdjustOp op = change->op;
        op.levels[0].inBlack = 0.3;
        const AdjustModel::Change reset = AdjustModel::resetLevels(op);
        QCOMPARE(reset.op.outBlack, 0.0);
        QCOMPARE(reset.op.outWhite, 1.0);
        QCOMPARE(reset.op.levels[0].inBlack, 0.0);
        QVERIFY(reset.key.isEmpty());          // a reset never merges with a slider drag
        QVERIFY(AdjustModel::resetLevels(neutral).op.isIdentity()); // and is a change even when nothing was set
    }

    // ---- curves
    void curvePointsAreSortedClampedCappedAndTheStraightLineIsNoCurve()
    {
        const AdjustOp neutral;
        const auto change = AdjustModel::setCurvePoints(neutral, 0, {point(0.8, 0.2), point(0.2, 0.9), point(2.0, -1.0)});
        QVERIFY(change);
        const auto &pts = change->op.curves[0];
        QCOMPARE(pts.size(), size_t(3));
        QCOMPARE(pts[0].x(), 0.2); // sorted by x
        QCOMPARE(pts[2].x(), 1.0); // clamped to 0..1
        QCOMPARE(pts[2].y(), 0.0);
        QCOMPARE(change->key, QStringLiteral("curves:0"));

        QVERIFY(!AdjustModel::setCurvePoints(neutral, 0, {point(0, 0), point(1, 1)})); // identity: already "no curve"
        AdjustOp withCurve = change->op;
        const auto cleared = AdjustModel::setCurvePoints(withCurve, 0, {point(0, 0), point(1, 1)});
        QVERIFY(cleared);
        QVERIFY(cleared->op.curves[0].empty());                                         // the straight line clears it

        QVERIFY(!AdjustModel::setCurvePoints(neutral, 7, {point(0.5, 0.2)}));            // no such channel

        QVariantList many;
        for (int i = 0; i < 30; ++i)
            many.append(point(i / 30.0, 0.5 + 0.01 * i));
        QCOMPARE(AdjustModel::setCurvePoints(neutral, 3, many)->op.curves[3].size(), size_t(16)); // at most 16

        QVERIFY(!AdjustModel::setCurvePoints(withCurve, 0, {point(0.8, 0.2), point(0.2, 0.9), point(2.0, -1.0)})); // same again
    }

    void pointsMayComeAsQtPointsToo()
    {
        const AdjustOp neutral;
        const QVariantList points{QVariant::fromValue(QPointF(0.2, 0.7)), QVariant::fromValue(QPointF(0.9, 0.1)), QVariant(QStringLiteral("junk"))};
        const auto change = AdjustModel::setCurvePoints(neutral, 1, points);
        QVERIFY(change);
        QCOMPARE(change->op.curves[1].size(), size_t(2)); // the junk entry is skipped
    }

    void curveSamplesAre256Values()
    {
        const QVariantList samples = AdjustModel::curveSamples({point(0, 0), point(0.5, 0.8), point(1, 1)});
        QCOMPARE(samples.size(), 256);
        QCOMPARE(samples.first().toDouble(), 0.0);
        QCOMPARE(samples.last().toDouble(), 1.0);
        QVERIFY(samples.at(128).toDouble() > 0.6); // above the straight line: the curve bends up
    }

    // ---- auto and the Grade stage
    void autoKindsAreRecognised()
    {
        QVERIFY(AdjustModel::autoKind(QStringLiteral("contrast")));
        QVERIFY(AdjustModel::autoKind(QStringLiteral("levels")));
        QVERIFY(AdjustModel::autoKind(QStringLiteral("color")));
        QVERIFY(!AdjustModel::autoKind(QStringLiteral("bogus")));
        QVERIFY(!AdjustModel::autoKind(QString()));
    }

    void gradeIgnoresTheDetailControls()
    {
        AdjustOp op;
        QVERIFY(!AdjustModel::gradeActive(op));
        op.sharpness = 0.5;
        op.clarity = 0.5;
        op.vignette = 0.5;
        op.grain = 0.5;
        QVERIFY(!AdjustModel::gradeActive(op)); // Detail runs on its own
        op.exposure = 0.1;
        QVERIFY(AdjustModel::gradeActive(op));
        AdjustOp negative;
        negative.negative = true;
        QVERIFY(AdjustModel::gradeActive(negative));
        AdjustOp curves;
        curves.curves[0] = {QPointF(0.2, 0.4), QPointF(0.8, 0.6)};
        QVERIFY(AdjustModel::gradeActive(curves));
    }

    // ---- looks
    void aPickedLookIsClampedAndAnUnknownOneIsNone()
    {
        const QString id = LookModel::list().first().toMap().value("id").toString();
        QVERIFY(!id.isEmpty());
        const LookModel::Picked high = LookModel::pick(id, 2.0);
        QCOMPARE(high.id, id);
        QCOMPARE(high.amount, 1.0);
        QCOMPARE(LookModel::pick(id, -1.0).amount, 0.0);
        const LookModel::Picked none = LookModel::pick(QStringLiteral("no-such-look"), 0.5);
        QVERIFY(none.id.isEmpty());
        QCOMPARE(none.amount, 0.5);
        QVERIFY(LookModel::pick(QString(), 0.5).id.isEmpty());
    }

    void aLookActsOnlyWhenItExistsAndHasAmount()
    {
        const QString id = LookModel::list().first().toMap().value("id").toString();
        QVERIFY(LookModel::active(id, 0.5));
        QVERIFY(!LookModel::active(id, 0.0));
        QVERIFY(!LookModel::active(QString(), 1.0));
        QVERIFY(!LookModel::active(QStringLiteral("nope"), 1.0));
    }

    void theLookMapCarriesWhatTheShaderReads()
    {
        const QString id = LookModel::list().first().toMap().value("id").toString();
        const QVariantMap map = LookModel::toMap(id, 0.6);
        for (const char *key : {"id", "amount", "shadows", "highlights", "saturation", "vibrance", "hue", "gradientOn",
                                "gradient0", "gradient1", "gradient2", "shadowOffset", "highlightOffset", "vignette",
                                "grain", "grainType", "grainMono"})
            QVERIFY2(map.contains(QLatin1String(key)), key);
        QCOMPARE(map.value("id").toString(), id);
        QCOMPARE(map.value("amount").toDouble(), 0.6);

        const QVariantMap none = LookModel::toMap(QString(), 0.6); // no look: amount 0 whatever was asked
        QCOMPARE(none.value("amount").toDouble(), 0.0);
        QCOMPARE(none.value("gradientOn").toDouble(), 0.0);
    }

    void theToneTableOfNoLookIsNeutral()
    {
        QVERIFY(LookModel::tone(QString()).isIdentity());
        QVERIFY(LookModel::tone(QStringLiteral("nope")).isIdentity());
    }

    void theListsAreFilled()
    {
        const QVariantList looks = LookModel::list();
        QVERIFY(looks.size() > 10);
        for (const QVariant &v : looks) {
            const QVariantMap m = v.toMap();
            QVERIFY(!m.value("id").toString().isEmpty());
            QVERIFY(!m.value("name").toString().isEmpty());
            QVERIFY(!m.value("group").toString().isEmpty());
        }
        QVERIFY(!LookModel::groups().isEmpty());
    }
};

QTEST_MAIN(TestAdjustModel)
#include "test_adjust_model.moc"
