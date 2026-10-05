#include "Blend.h"
#include "EffectsCommon.h"

// The "Luz" family: bokeh, lens flares, light leaks, beams, dust and sparkles, drawn procedurally.

namespace core::edit::fxk {

void addLightSpecs(std::vector<EffectSpec> &)
{
}

bool renderLight(const Job &, const QString &, const QImage &, const EffectValues &, double, QImage &)
{
    return false;
}

} // namespace core::edit::fxk
