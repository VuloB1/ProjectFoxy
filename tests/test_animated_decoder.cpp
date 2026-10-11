#include <QtTest>
#include "decoders/AnimatedDecoder.h"

// FIXTURES_DIR is injected by CMake (see tests/CMakeLists.txt) as the
// absolute path to tests/fixtures, so this test works regardless of the
// runner's current working directory.
#ifndef FIXTURES_DIR
#define FIXTURES_DIR "."
#endif

namespace {
// Puts the process-wide limits back when a test that lowered them ends.
struct LimitsGuard {
    core::AnimatedDecoder::Limits saved = core::AnimatedDecoder::limits();
    ~LimitsGuard() { core::AnimatedDecoder::setLimits(saved); }
};
} // namespace

class TestAnimatedDecoder : public QObject {
    Q_OBJECT
private slots:
    void detectsAnimatedGif()
    {
        core::AnimatedDecoder decoder;
        QVERIFY(decoder.canDecode(QStringLiteral(FIXTURES_DIR "/test_animation.gif")));
    }

    void detectsAnimatedPng()
    {
        core::AnimatedDecoder decoder;
        QVERIFY(decoder.canDecode(QStringLiteral(FIXTURES_DIR "/test_animation.png")));
    }

    // A single-frame GIF/PNG must NOT be claimed by this decoder - it has to
    // keep falling through to VipsDecoder exactly as before (see
    // DecoderRegistry's constructor comment on why ordering matters here).
    void declinesStaticGif()
    {
        core::AnimatedDecoder decoder;
        QVERIFY(!decoder.canDecode(QStringLiteral(FIXTURES_DIR "/test_static.gif")));
    }

    void declinesStaticPng()
    {
        core::AnimatedDecoder decoder;
        QVERIFY(!decoder.canDecode(QStringLiteral(FIXTURES_DIR "/test_static.png")));
    }

    void decodesAllGifFrames()
    {
        core::AnimatedDecoder decoder;
        const core::DecodeResult result = decoder.decode(QStringLiteral(FIXTURES_DIR "/test_animation.gif"));
        QVERIFY(result.ok);
        QCOMPARE(result.frames.size(), 4);
        QCOMPARE(result.frameDelaysMs.size(), 4);
        for (const QImage &frame : result.frames)
            QVERIFY(!frame.isNull());
        for (int delay : result.frameDelaysMs)
            QVERIFY(delay > 0);
    }

    // This is the exact case that regressed once already: a preview/
    // thumbnail request (maxSize valid) must decode frame 0's actual pixel
    // data, not an empty/truncated buffer - see the fix in
    // AnimatedDecoder.cpp's fcTL handling ("Not enough image data").
    void decodesFirstApngFrameOnlyForPreview()
    {
        core::AnimatedDecoder decoder;
        const core::DecodeResult result = decoder.decode(
            QStringLiteral(FIXTURES_DIR "/test_animation.png"), QSize(64, 64));
        QVERIFY(result.ok);
        QVERIFY(!result.image.isNull());
        QVERIFY(result.frames.isEmpty()); // preview path never populates the full frame list
    }

    void decodesAllApngFramesWithCompositing()
    {
        core::AnimatedDecoder decoder;
        const core::DecodeResult result = decoder.decode(QStringLiteral(FIXTURES_DIR "/test_animation.png"));
        QVERIFY(result.ok);
        QCOMPARE(result.frames.size(), 4);
        QCOMPARE(result.frameDelaysMs.size(), 4);
        for (const QImage &frame : result.frames) {
            QVERIFY(!frame.isNull());
            QCOMPARE(frame.size(), QSize(200, 200));
        }
        // The 4 fixture frames are distinct flat colors (see
        // make_test_anims.py) - if compositing/frame-data extraction were
        // subtly broken (e.g. every frame silently reusing frame 0's bytes),
        // this would still pass frame-count/size checks alone, so also
        // confirm consecutive frames actually differ pixel-for-pixel.
        QVERIFY(result.frames[0] != result.frames[1]);
        QVERIFY(result.frames[1] != result.frames[2]);
        QVERIFY(result.frames[2] != result.frames[3]);
    }

    // ---- memory limits (a valid file can still be absurdly big)

    // The frames that fit are kept, the first one always; the picture still opens.
    void anAnimationOverTheByteLimitIsCutShort()
    {
        LimitsGuard guard;
        auto limits = guard.saved;
        limits.maxAnimationBytes = 200 * 200 * 4 * 2; // two 200x200 frames
        core::AnimatedDecoder::setLimits(limits);
        core::AnimatedDecoder decoder;
        for (const char *file : {"/test_animation.gif", "/test_animation.png"}) {
            const core::DecodeResult result = decoder.decode(QString(FIXTURES_DIR) + QLatin1String(file));
            QVERIFY2(result.ok, file);
            QCOMPARE(result.frames.size(), 2);
            QCOMPARE(result.frameDelaysMs.size(), 2);
        }
    }

    void theFrameCountLimitAlsoCutsAnAnimation()
    {
        LimitsGuard guard;
        auto limits = guard.saved;
        limits.maxFrames = 3;
        core::AnimatedDecoder::setLimits(limits);
        core::AnimatedDecoder decoder;
        const core::DecodeResult result = decoder.decode(QStringLiteral(FIXTURES_DIR "/test_animation.gif"));
        QVERIFY(result.ok);
        QCOMPARE(result.frames.size(), 3);
    }

    void aCanvasOverTheLimitIsRefused()
    {
        LimitsGuard guard;
        auto limits = guard.saved;
        limits.maxCanvasPixels = 100 * 100; // the fixtures are 200x200
        core::AnimatedDecoder::setLimits(limits);
        core::AnimatedDecoder decoder;
        for (const char *file : {"/test_animation.gif", "/test_animation.png"}) {
            const core::DecodeResult result = decoder.decode(QString(FIXTURES_DIR) + QLatin1String(file));
            QVERIFY2(!result.ok, file);
            QVERIFY(!result.error.isEmpty());
        }
    }

    void aFileOverTheInputLimitIsRefused()
    {
        LimitsGuard guard;
        auto limits = guard.saved;
        limits.maxInputBytes = 100;
        core::AnimatedDecoder::setLimits(limits);
        core::AnimatedDecoder decoder;
        QVERIFY(!decoder.decode(QStringLiteral(FIXTURES_DIR "/test_animation.png")).ok);
    }

    void aPreviewIsAlwaysOneFrameWhateverTheLimits()
    {
        LimitsGuard guard;
        auto limits = guard.saved;
        limits.maxAnimationBytes = 1; // not even one frame "fits", yet the first is always kept
        core::AnimatedDecoder::setLimits(limits);
        core::AnimatedDecoder decoder;
        const core::DecodeResult result = decoder.decode(QStringLiteral(FIXTURES_DIR "/test_animation.gif"));
        QVERIFY(result.ok);
        QVERIFY(result.frames.isEmpty()); // a single frame is a still picture, not an animation
        QVERIFY(!result.image.isNull());
    }
};

QTEST_MAIN(TestAnimatedDecoder)
#include "test_animated_decoder.moc"
