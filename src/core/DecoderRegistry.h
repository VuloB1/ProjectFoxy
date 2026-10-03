#pragma once

#include "decoders/IImageDecoder.h"
#include <vector>

namespace core {

// Owns one instance of each IImageDecoder and picks the right one per file
// extension. New formats are added by registering a new decoder here -
// nothing else in the codebase needs to know about format specifics.
class DecoderRegistry {
public:
    static DecoderRegistry &instance();

    IImageDecoderPtr decoderFor(const QString &filePath) const;
    QStringList allSupportedExtensions() const;

    // Puts `decoder` in front of the built-in ones, so it is asked first. Meant for
    // tests that need a decoder they control (slow, failing...); call it before any
    // loading starts - the list is not locked against concurrent decodes.
    void registerDecoder(IImageDecoderPtr decoder);

private:
    DecoderRegistry();
    std::vector<IImageDecoderPtr> m_decoders;
};

} // namespace core
