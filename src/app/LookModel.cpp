#include "LookModel.h"

#include "Translate.h"
#include "edit/Looks.h"

#include <QVector3D>
#include <algorithm>
#include <array>

namespace LookModel {

Picked pick(const QString &presetId, double intensity)
{
    return {core::edit::findLook(presetId) ? presetId : QString(), std::clamp(intensity, 0.0, 1.0)};
}

bool active(const QString &id, double amount)
{
    return core::edit::findLook(id) != nullptr && amount > 0.0;
}

core::edit::AdjustOp tone(const QString &id)
{
    const core::edit::LookSpec *spec = core::edit::findLook(id);
    return spec ? spec->tone : core::edit::AdjustOp();
}

QVariantMap toMap(const QString &id, double amount)
{
    const core::edit::LookSpec *spec = core::edit::findLook(id);
    const core::edit::AdjustOp toneOp = spec ? spec->tone : core::edit::AdjustOp();
    const core::edit::GradeExtras extras = spec ? spec->extras : core::edit::GradeExtras();
    auto vec = [](const std::array<double, 3> &a) { return QVector3D(float(a[0]), float(a[1]), float(a[2])); };

    QVariantMap map;
    map.insert(QStringLiteral("id"), id);
    map.insert(QStringLiteral("amount"), spec ? amount : 0.0);
    map.insert(QStringLiteral("shadows"), toneOp.shadows);
    map.insert(QStringLiteral("highlights"), toneOp.highlights);
    map.insert(QStringLiteral("saturation"), toneOp.saturation);
    map.insert(QStringLiteral("vibrance"), toneOp.vibrance);
    map.insert(QStringLiteral("hue"), toneOp.hue);
    map.insert(QStringLiteral("gradientOn"), extras.gradient ? 1.0 : 0.0);
    map.insert(QStringLiteral("gradient0"), vec(extras.stops[0]));
    map.insert(QStringLiteral("gradient1"), vec(extras.stops[1]));
    map.insert(QStringLiteral("gradient2"), vec(extras.stops[2]));
    map.insert(QStringLiteral("shadowOffset"), vec(extras.shadowOffset));
    map.insert(QStringLiteral("highlightOffset"), vec(extras.highlightOffset));
    map.insert(QStringLiteral("vignette"), extras.vignette);
    map.insert(QStringLiteral("grain"), extras.grain);
    map.insert(QStringLiteral("grainType"), extras.grainType);
    map.insert(QStringLiteral("grainMono"), extras.grainMono ? 1.0 : 0.0);
    return map;
}

QVariantList list()
{
    QVariantList result;
    for (const core::edit::LookSpec &look : core::edit::allLooks()) {
        QVariantMap m;
        m.insert(QStringLiteral("id"), look.id);
        m.insert(QStringLiteral("name"), core::tr(look.name));
        m.insert(QStringLiteral("group"), look.group);
        result.append(m);
    }
    return result;
}

QVariantList groups()
{
    QVariantList result;
    for (const core::edit::LookGroup &group : core::edit::lookGroups()) {
        QVariantMap m;
        m.insert(QStringLiteral("id"), group.id);
        m.insert(QStringLiteral("name"), core::tr(group.name));
        result.append(m);
    }
    return result;
}

} // namespace LookModel
