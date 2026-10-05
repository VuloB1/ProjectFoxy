#include "Blend.h"
#include "EffectsCommon.h"

// Effects that move pixels around and may change the size of the picture: stretch, perspective,
// lens correction.

namespace core::edit::fxk {

void addGeometrySpecs(std::vector<EffectSpec> &)
{
}

bool renderGeometry(const Job &, const QString &, const QImage &, const EffectValues &, double, QImage &)
{
    return false;
}

QSize geometryOutputSize(const QString &, const EffectValues &, QSize input)
{
    return input;
}

} // namespace core::edit::fxk
