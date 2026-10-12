#include "AdjustModel.h"

#include "edit/AdjustMath.h"

#include <QPointF>
#include <algorithm>

using core::edit::AdjustOp;

namespace AdjustModel {

QVariantMap toMap(const AdjustOp &op)
{
    QVariantMap map;
    map.insert(QStringLiteral("exposure"), op.exposure);
    map.insert(QStringLiteral("brightness"), op.brightness);
    map.insert(QStringLiteral("contrast"), op.contrast);
    map.insert(QStringLiteral("highlights"), op.highlights);
    map.insert(QStringLiteral("shadows"), op.shadows);
    map.insert(QStringLiteral("whites"), op.whites);
    map.insert(QStringLiteral("blacks"), op.blacks);
    map.insert(QStringLiteral("gamma"), op.gamma);
    map.insert(QStringLiteral("temperature"), op.temperature);
    map.insert(QStringLiteral("tint"), op.tint);
    map.insert(QStringLiteral("saturation"), op.saturation);
    map.insert(QStringLiteral("vibrance"), op.vibrance);
    map.insert(QStringLiteral("hue"), op.hue);
    map.insert(QStringLiteral("clarity"), op.clarity);
    map.insert(QStringLiteral("sharpness"), op.sharpness);
    map.insert(QStringLiteral("vignette"), op.vignette);
    map.insert(QStringLiteral("grain"), op.grain);
    map.insert(QStringLiteral("grainType"), op.grainType);
    map.insert(QStringLiteral("grainMono"), op.grainMono);
    map.insert(QStringLiteral("negative"), op.negative);
    map.insert(QStringLiteral("outBlack"), op.outBlack);
    map.insert(QStringLiteral("outWhite"), op.outWhite);

    QVariantList levels;
    for (const auto &lv : op.levels) {
        QVariantMap m;
        m.insert(QStringLiteral("inBlack"), lv.inBlack);
        m.insert(QStringLiteral("gamma"), lv.gamma);
        m.insert(QStringLiteral("inWhite"), lv.inWhite);
        levels.append(m);
    }
    map.insert(QStringLiteral("levels"), levels);

    QVariantList curves;
    for (const auto &curve : op.curves) {
        QVariantList pts;
        for (const QPointF &p : curve) {
            QVariantMap m;
            m.insert(QStringLiteral("x"), p.x());
            m.insert(QStringLiteral("y"), p.y());
            pts.append(m);
        }
        // fromValue on purpose: QVariantList::append(QVariantList) would
        // CONCATENATE the points into `curves` instead of nesting one list
        // per channel, and the editor would read a single point back.
        curves.append(QVariant::fromValue(pts));
    }
    map.insert(QStringLiteral("curves"), curves);
    return map;
}

std::optional<Change> setParam(const AdjustOp &current, const QString &name, double value)
{
    struct Param {
        const char *name;
        double AdjustOp::*field;
        double lo;
        double hi;
    };
    static const Param kParams[] = {
        {"exposure", &AdjustOp::exposure, -1.0, 1.0},
        {"brightness", &AdjustOp::brightness, -1.0, 1.0},
        {"contrast", &AdjustOp::contrast, -1.0, 1.0},
        {"highlights", &AdjustOp::highlights, -1.0, 1.0},
        {"shadows", &AdjustOp::shadows, -1.0, 1.0},
        {"whites", &AdjustOp::whites, -1.0, 1.0},
        {"blacks", &AdjustOp::blacks, -1.0, 1.0},
        {"gamma", &AdjustOp::gamma, -1.0, 1.0},
        {"temperature", &AdjustOp::temperature, -1.0, 1.0},
        {"tint", &AdjustOp::tint, -1.0, 1.0},
        {"saturation", &AdjustOp::saturation, -1.0, 1.0},
        {"vibrance", &AdjustOp::vibrance, -1.0, 1.0},
        {"hue", &AdjustOp::hue, -1.0, 1.0},
        {"clarity", &AdjustOp::clarity, -1.0, 1.0},
        {"sharpness", &AdjustOp::sharpness, 0.0, 1.0},
        {"vignette", &AdjustOp::vignette, -1.0, 1.0},
        {"grain", &AdjustOp::grain, 0.0, 1.0},
    };
    for (const Param &p : kParams) {
        if (name != QLatin1String(p.name))
            continue;
        const double clamped = std::clamp(value, p.lo, p.hi);
        if (current.*(p.field) == clamped)
            return std::nullopt;
        AdjustOp op = current;
        op.*(p.field) = clamped;
        return Change{op, QStringLiteral("p:") + name};
    }
    return std::nullopt;
}

std::optional<Change> setGrainType(const AdjustOp &current, int type)
{
    type = std::clamp(type, 0, 3);
    if (current.grainType == type)
        return std::nullopt;
    AdjustOp op = current;
    op.grainType = type;
    return Change{op, QStringLiteral("grainType")};
}

std::optional<Change> setGrainMono(const AdjustOp &current, bool mono)
{
    if (current.grainMono == mono)
        return std::nullopt;
    AdjustOp op = current;
    op.grainMono = mono;
    return Change{op, QStringLiteral("grainMono")};
}

std::optional<Change> setNegative(const AdjustOp &current, bool on)
{
    if (current.negative == on)
        return std::nullopt;
    AdjustOp op = current;
    op.negative = on;
    return Change{op, QStringLiteral("negative")};
}

std::optional<Change> setLevels(const AdjustOp &current, int channel, double inBlack, double gamma, double inWhite)
{
    if (channel < 0 || channel > 3)
        return std::nullopt;
    core::edit::LevelsChannel lv;
    lv.inBlack = std::clamp(inBlack, 0.0, 0.98);
    lv.inWhite = std::clamp(inWhite, lv.inBlack + 0.02, 1.0);
    lv.gamma = std::clamp(gamma, 0.1, 9.99);
    const auto &cur = current.levels[channel];
    if (cur.inBlack == lv.inBlack && cur.gamma == lv.gamma && cur.inWhite == lv.inWhite)
        return std::nullopt;
    AdjustOp op = current;
    op.levels[channel] = lv;
    return Change{op, QStringLiteral("levels:%1").arg(channel)};
}

std::optional<Change> setOutputLevels(const AdjustOp &current, double black, double white)
{
    const double b = std::clamp(black, 0.0, 0.98);
    const double w = std::clamp(white, b + 0.02, 1.0);
    if (current.outBlack == b && current.outWhite == w)
        return std::nullopt;
    AdjustOp op = current;
    op.outBlack = b;
    op.outWhite = w;
    return Change{op, QStringLiteral("outputLevels")};
}

namespace {

// Accepts what QML hands over for a point: a {x, y} object or a Qt.point().
bool pointFromVariant(const QVariant &v, QPointF *out)
{
    if (v.typeId() == QMetaType::QVariantMap) {
        const QVariantMap m = v.toMap();
        *out = QPointF(m.value(QStringLiteral("x")).toDouble(), m.value(QStringLiteral("y")).toDouble());
        return true;
    }
    if (v.canConvert<QPointF>()) {
        *out = v.toPointF();
        return true;
    }
    return false;
}

} // namespace

std::optional<Change> setCurvePoints(const AdjustOp &current, int channel, const QVariantList &points)
{
    if (channel < 0 || channel > 3)
        return std::nullopt;

    core::edit::CurvePoints curve;
    for (const QVariant &v : points) {
        QPointF p;
        if (!pointFromVariant(v, &p))
            continue;
        curve.emplace_back(std::clamp(p.x(), 0.0, 1.0), std::clamp(p.y(), 0.0, 1.0));
        if (curve.size() >= 16)
            break;
    }
    std::sort(curve.begin(), curve.end(), [](const QPointF &a, const QPointF &b) { return a.x() < b.x(); });
    if (core::edit::isIdentityCurve(curve))
        curve.clear();

    if (current.curves[channel] == curve)
        return std::nullopt;
    AdjustOp op = current;
    op.curves[channel] = curve;
    return Change{op, QStringLiteral("curves:%1").arg(channel)};
}

Change resetLevels(const AdjustOp &current)
{
    AdjustOp op = current;
    op.levels = {};
    op.outBlack = 0.0;
    op.outWhite = 1.0;
    return Change{op, QString()};
}

QVariantList curveSamples(const QVariantList &points)
{
    core::edit::CurvePoints curve;
    for (const QVariant &v : points) {
        QPointF p;
        if (pointFromVariant(v, &p))
            curve.push_back(p);
    }
    const auto samples = core::edit::sampleCurve(curve);
    QVariantList list;
    list.reserve(256);
    for (double s : samples)
        list.append(s);
    return list;
}

std::optional<core::edit::AutoAdjustKind> autoKind(const QString &kind)
{
    if (kind == QLatin1String("contrast"))
        return core::edit::AutoAdjustKind::Contrast;
    if (kind == QLatin1String("levels"))
        return core::edit::AutoAdjustKind::Levels;
    if (kind == QLatin1String("color"))
        return core::edit::AutoAdjustKind::Color;
    return std::nullopt;
}

bool gradeActive(const AdjustOp &op)
{
    // Clarity, sharpness, vignette and grain belong to the Detail stage, which always runs.
    AdjustOp grade = op;
    grade.clarity = 0.0;
    grade.sharpness = 0.0;
    grade.vignette = 0.0;
    grade.grain = 0.0;
    return !grade.isIdentity();
}

} // namespace AdjustModel
