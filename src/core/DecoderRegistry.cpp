#include "DecoderRegistry.h"
#include "decoders/VipsDecoder.h"
#include "decoders/RawDecoder.h"
#include "decoders/HeifDecoder.h"
#include "decoders/AnimatedDecoder.h"
#include "decoders/QtImageDecoder.h"

namespace core {

DecoderRegistry &DecoderRegistry::instance()
{
    static DecoderRegistry registry;
    return registry;
}

DecoderRegistry::DecoderRegistry()
{
    // Order matters here: AnimatedDecoder must be tried before the others, since
    // it and QtImageDecoder/VipsDecoder can all claim "gif"/"png" - its
    // canDecode() only actually accepts a file that's genuinely multi-frame, so
    // a still gif falls through to QtImageDecoder and a still png to VipsDecoder.
    m_decoders.push_back(std::make_shared<RawDecoder>());
    m_decoders.push_back(std::make_shared<HeifDecoder>());
    m_decoders.push_back(std::make_shared<AnimatedDecoder>());
    m_decoders.push_back(std::make_shared<QtImageDecoder>());
    m_decoders.push_back(std::make_shared<VipsDecoder>());
}

void DecoderRegistry::registerDecoder(IImageDecoderPtr decoder)
{
    m_decoders.insert(m_decoders.begin(), std::move(decoder));
}

IImageDecoderPtr DecoderRegistry::decoderFor(const QString &filePath) const
{
    for (const auto &decoder : m_decoders) {
        if (decoder->canDecode(filePath))
            return decoder;
    }
    return nullptr;
}

QStringList DecoderRegistry::allSupportedExtensions() const
{
    QStringList extensions;
    for (const auto &decoder : m_decoders) {
        for (const QString &ext : decoder->supportedExtensions()) {
            // AnimatedDecoder and VipsDecoder both claim "gif"/"png" (see the
            // constructor comment above) - dedupe so callers building a file
            // dialog filter/extension list don't show "*.gif *.gif".
            if (!extensions.contains(ext))
                extensions << ext;
        }
    }
    return extensions;
}

} // namespace core
