#include "TextRecognizer.h"

#include "AppController.h"
#include "OcrBackend.h"
#include "OcrLanguages.h"
#include "TextParagraphs.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QPointer>
#include <QThreadPool>

#if defined(Q_OS_WIN) && !defined(PROJECTFOXY_TESSERACT)
#define PROJECTFOXY_WINDOWS_OCR
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Globalization.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Media.Ocr.h>
#include <winrt/Windows.Storage.Streams.h>

namespace {

OcrFound recognise(QImage image)
{
    using namespace winrt::Windows::Media::Ocr;
    using namespace winrt::Windows::Graphics::Imaging;
    OcrFound found;
    try {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        OcrEngine engine = OcrEngine::TryCreateFromUserProfileLanguages();
        if (!engine)
            engine = OcrEngine::TryCreateFromLanguage(winrt::Windows::Globalization::Language(L"en-US"));
        if (!engine) {
            found.noLanguage = true;
            return found;
        }
        // The recogniser takes pictures up to a size; a bigger one is read reduced and the boxes brought back.
        const int limit = int(OcrEngine::MaxImageDimension());
        double back = 1.0;
        if (qMax(image.width(), image.height()) > limit) {
            back = double(qMax(image.width(), image.height())) / limit;
            image = image.scaled(limit, limit, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
        image = image.convertToFormat(QImage::Format_ARGB32_Premultiplied); // B, G, R, A in memory
        winrt::Windows::Storage::Streams::DataWriter writer;
        for (int y = 0; y < image.height(); ++y)
            writer.WriteBytes(winrt::array_view<const uint8_t>(image.constScanLine(y), image.constScanLine(y) + image.width() * 4));
        const SoftwareBitmap bitmap = SoftwareBitmap::CreateCopyFromBuffer(writer.DetachBuffer(), BitmapPixelFormat::Bgra8,
                                                                           image.width(), image.height(), BitmapAlphaMode::Premultiplied);
        const OcrResult result = engine.RecognizeAsync(bitmap).get();
        const QString tag = QString::fromStdWString(std::wstring(engine.RecognizerLanguage().LanguageTag()));
        const bool noSpaces = tag.startsWith(QLatin1String("zh")) || tag.startsWith(QLatin1String("ja")); // their words are not separated
        int line = 0;
        std::vector<QRectF> boxes; // one per line, to group them into paragraphs
        for (const OcrLine &l : result.Lines()) {
            QRectF box;
            for (const OcrWord &w : l.Words()) {
                const auto r = w.BoundingRect();
                QVariantMap word;
                word[QStringLiteral("text")] = QString::fromStdWString(std::wstring(w.Text()));
                word[QStringLiteral("x")] = r.X * back;
                word[QStringLiteral("y")] = r.Y * back;
                word[QStringLiteral("w")] = r.Width * back;
                word[QStringLiteral("h")] = r.Height * back;
                word[QStringLiteral("line")] = line;
                word[QStringLiteral("joined")] = noSpaces;
                found.words.append(word);
                box = box.united(QRectF(r.X * back, r.Y * back, r.Width * back, r.Height * back));
            }
            boxes.push_back(box);
            ++line;
        }
        const std::vector<int> paragraph = core::paragraphsOf(boxes);
        for (QVariant &v : found.words) {
            QVariantMap word = v.toMap();
            word[QStringLiteral("para")] = paragraph[size_t(word[QStringLiteral("line")].toInt())];
            v = word;
        }
    } catch (const winrt::hresult_error &) {
        found.words.clear(); // whatever failed, the answer is "no text"
    }
    return found;
}

} // namespace
#endif

TextRecognizer::TextRecognizer(AppController *controller, OcrLanguages *languages, QObject *parent)
    : QObject(parent), m_controller(controller), m_languages(languages)
{
}

bool TextRecognizer::available() const
{
#if defined(PROJECTFOXY_WINDOWS_OCR) || defined(PROJECTFOXY_TESSERACT)
    return true;
#else
    return false;
#endif
}

void TextRecognizer::scan()
{
    clear();
#if defined(PROJECTFOXY_WINDOWS_OCR) || defined(PROJECTFOXY_TESSERACT)
    const QImage image = m_controller->displayedImage();
    if (image.isNull())
        return;
    m_busy = true;
    emit busyChanged();
    const int job = ++m_job;
    QPointer<TextRecognizer> self(this);
    const QStringList languages = m_languages->activeLanguages(); // Tesseract's; the recogniser of Windows reads what Windows has
    const QStringList dirs = m_languages->dataDirs();
    QThreadPool::globalInstance()->start([self, image, job, languages, dirs]() {
#ifdef PROJECTFOXY_TESSERACT
        const OcrFound found = recogniseWithTesseract(image, languages, dirs);
#else
        Q_UNUSED(languages) Q_UNUSED(dirs)
        const OcrFound found = recognise(image);
#endif
        QMetaObject::invokeMethod(qApp, [self, found, job]() {
            if (!self || job != self->m_job)
                return;
            self->m_busy = false;
            self->m_words = found.words;
            self->m_problem = found.noLanguage ? QStringLiteral("language") : found.words.isEmpty() ? QStringLiteral("none") : QString();
            emit self->busyChanged();
            emit self->wordsChanged();
            emit self->problemChanged();
        });
    });
#endif
}

void TextRecognizer::clear()
{
    ++m_job;
    const bool wasBusy = m_busy;
    m_busy = false;
    const bool hadResult = !m_words.isEmpty() || !m_problem.isEmpty();
    m_words.clear();
    m_problem.clear();
    if (wasBusy)
        emit busyChanged();
    if (hadResult) {
        emit wordsChanged();
        emit problemChanged();
    }
}

void TextRecognizer::copyText(const QString &text) const
{
    QGuiApplication::clipboard()->setText(text);
}
