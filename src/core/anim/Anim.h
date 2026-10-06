#pragma once

// Animated pictures: building the frames (fitting every picture to one size, the transitions between
// them, the order they play in) and writing them as an animated GIF, APNG or WebP.
//
// Everything works on a FrameSource, which hands out one finished canvas-sized RGBA picture at a time, so
// an animation of hundreds of frames never has to be in memory at once.

#include <QColor>
#include <QImage>
#include <QIODevice>
#include <QSize>
#include <QString>
#include <functional>
#include <vector>

namespace core::anim {

// ---- frames in, files out --------------------------------------------------------------------------------------

class FrameSource {
public:
    virtual ~FrameSource() = default;
    virtual int count() const = 0;
    virtual QSize size() const = 0;
    // Frame `i` (0-based) as Format_RGBA8888 of size(), and how long it stays on screen in milliseconds.
    // Called once per frame, in order, by the encoders.
    virtual QImage frame(int i, int *delayMs) = 0;
};

// Called after each frame; returning false cancels the export.
using ProgressFn = std::function<bool(int done, int total)>;

struct GifOptions {
    int colors = 256;            // 2..256 (one of them is kept for "transparent")
    bool dither = true;          // Floyd-Steinberg: smoother gradients, noisier flat areas, larger files
    bool localPalettes = false;  // one palette per frame instead of one for all: better colours, larger file
    bool optimize = true;        // write only the part of each frame that changed
    int loops = 0;               // 0 = forever
};

struct ApngOptions {
    bool optimize = true;        // write only the part of each frame that changed
    int loops = 0;               // 0 = forever
};

struct WebpOptions {
    bool lossless = false;
    int quality = 80;            // 0..100 (lossy), or how hard to compress (lossless)
    int loops = 0;               // 0 = forever
};

// Each returns false (with `error` set, unless it was cancelled) when it could not finish.
bool writeGif(FrameSource &frames, const GifOptions &options, QIODevice &out, const ProgressFn &progress = nullptr,
              QString *error = nullptr);
bool writeApng(FrameSource &frames, const ApngOptions &options, QIODevice &out, const ProgressFn &progress = nullptr,
               QString *error = nullptr);
bool writeWebp(FrameSource &frames, const WebpOptions &options, QIODevice &out, const ProgressFn &progress = nullptr,
               QString *error = nullptr);

// ---- building the frames ---------------------------------------------------------------------------------------

enum class Fit {
    Contain, // the whole picture, with the background showing where it does not reach
    Cover,   // fills the frame, cutting what sticks out
    Stretch, // fills the frame, changing the proportions
};

enum class Transition {
    None,
    Fade,
    SlideLeft,  // the next picture comes in from the right, pushing the old one out to the left
    SlideRight,
    SlideUp,
    SlideDown,
    Zoom,       // the old one grows and fades while the new one settles in
};

struct Settings {
    QSize size = QSize(640, 480);
    Fit fit = Fit::Contain;
    QColor background = QColor(0, 0, 0);
    bool transparentBackground = false;
    Transition transition = Transition::None;
    int transitionMs = 400;      // how long each transition takes
    int transitionSteps = 6;     // how many frames it is made of
    bool transitionOnLoop = true; // also from the last picture back to the first
    bool reverse = false;
    bool pingPong = false;       // forward, then back (without repeating the ends)
    double speed = 1.0;          // 2 = twice as fast
};

// One frame of the finished animation: a picture held still, or a point (t, 0..1) of a transition from
// picture `a` to picture `b`. Indices are positions in the list of pictures.
struct PlanStep {
    int a = 0;
    int b = 0;
    double t = 0.0;
    int delayMs = 100;
    bool isStill() const { return a == b || t <= 0.0; }
};

// What is done to one picture once it is fitted to the canvas: an effect from the Efectos catalogue (one of its
// presets, mixed in by some amount) and a line of text over it. Both are in proportions of the canvas, so the same
// style looks the same in the small preview and in the finished file.
struct TextOverlay {
    QString text;                  // may have several lines
    QString family;                // "" = the application's font
    double x = 0.5, y = 0.88;      // where the middle of the text is, 0..1 of the canvas
    double size = 9.0;             // height of a line, % of the canvas' short side
    bool bold = true;
    QColor color = QColor(255, 255, 255);
    bool outline = true;
    QColor outlineColor = QColor(0, 0, 0);
    bool operator==(const TextOverlay &) const = default;
};

struct FrameStyle {
    QString effectId;              // "" = none
    int effectPreset = -1;         // -1 = the effect's own defaults
    double effectMix = 1.0;        // 0..1
    TextOverlay text;
    // -1: the text goes where `text.x/y` say. 0..3: the meme caption of the Efectos catalogue (one of its presets: a band
    // above or below the picture, or the text over it) - the picture gets the room the band leaves.
    int caption = -1;
    QColor bandColor = QColor(255, 255, 255);
    bool isPlain() const { return (effectId.isEmpty() || effectMix <= 0.0) && text.text.trimmed().isEmpty(); }
    // Equal styles give equal strings: for caches.
    QString signature() const;
    bool operator==(const FrameStyle &) const = default;
};

// Whether `effectId` can be used on a frame: it keeps the picture's size and does not take long.
bool effectUsableOnFrames(const QString &effectId);

// `fitted` with the style applied (it is returned as it is when there is nothing to do).
QImage decorate(const QImage &fitted, const FrameStyle &style);

// One finished frame: `source` fitted to the canvas of `settings` and given its style. What `decorate` cannot do
// alone (a caption band takes room from the picture) happens here, so use this one for frames.
QImage renderFrame(const QImage &source, const Settings &settings, const FrameStyle &style);

// The frames the animation is made of, in order, from how long each picture stays.
std::vector<PlanStep> buildPlan(const std::vector<int> &holdMs, const Settings &settings);

// `source` fitted into a canvas of settings.size.
QImage fitToCanvas(const QImage &source, const Settings &settings);

// The canvas for one step, from the two (already fitted) pictures it involves.
QImage renderStep(const QImage &fittedA, const QImage &fittedB, const PlanStep &step, const Settings &settings);

// The 5/5/5-bit colour quantiser the GIF writer uses, exposed for tests: a palette of at most `colors`
// entries (as 0xRRGGBB) that stands for the opaque pixels of `images`.
std::vector<unsigned> quantizePalette(const std::vector<QImage> &images, int colors);

} // namespace core::anim
