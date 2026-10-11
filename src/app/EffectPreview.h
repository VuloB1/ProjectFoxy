#pragma once

#include "edit/Effects.h"
#include "edit/Operations.h"

#include <QImage>
#include <QObject>
#include <QStringList>
#include <QThreadPool>
#include <QTimer>
#include <QVariantList>
#include <atomic>
#include <functional>
#include <memory>

// Efectos: the preview of one-shot effects (blurs, stylizing, distortions - see core/edit/Effects.h) over the whole
// photo, until the caller makes it a real edit or throws it away. Owns the picked effect and its sliders, the
// calculations (a quick approximation on a shrunken copy, then the exact one on the real pixels once the sliders
// rest) and the pool they run on. It knows nothing of the edit history or of what is on screen: the picture it
// starts from comes from `base`, and what it calculates leaves as pictureReady().
//
// A "generation" is one set of parameters: any change starts a new one, which cancels the previous one's work (its
// cancel flag) so stale results are never shown.
class EffectPreview : public QObject {
    Q_OBJECT
public:
    // `base` returns the picture the effect is applied to (the structural result of the edit history; a null image
    // when nothing is open).
    using BaseProvider = std::function<QImage()>;
    explicit EffectPreview(BaseProvider base, QObject *parent = nullptr);
    ~EffectPreview() override; // cancels what is running and waits for it: the workers hold `this`

    // ---- the catalogue (names go through core::tr)
    static QVariantList list();
    static QVariantList groups();
    QVariantList params() const;
    QStringList presets() const;
    QVariantList overlays() const;
    QVariantList values() const;
    bool usesText() const;

    // ---- what is picked
    bool active() const { return !m_id.isEmpty(); }
    QString id() const { return m_id; }
    double mix() const { return m_mix; }
    QString text() const { return m_text; }
    // A calculation is running or about to start.
    bool busy() const { return m_jobsRunning > 0 || m_debounce.isActive(); }

    // ---- commands. They all start a new preview where it applies.
    void select(const QString &id); // a known effect id: previews it with its default values
    void setValue(int index, qreal value);
    void setMix(qreal mix);
    void setText(const QString &text);
    void setValues(const QVariantList &values); // every slider at once (one preview, not one per slider)
    void applyPreset(int index);
    void resetValues();
    // Forgets the effect (cancelling any calculation in flight) WITHOUT touching the canvas. True when a preview
    // (not the real picture) was on screen, i.e. the caller has to put the real picture back.
    bool drop();
    // The picture the previews were shrunk from changed: the cached shrunken copy is stale.
    void invalidateProxy() { m_proxy = QImage(); }

    // ---- making it permanent / saving it
    core::edit::EffectOp operation() const { return {m_id, m_values, m_mix, m_text}; }
    // The exact full-size result for the current settings is already calculated.
    bool exactReady() const { return m_fullValid && m_cancel && !m_cancel->load(); }
    QImage exactResult() const { return m_full; }
    // `picture` with the effect applied: the exact result when it exists, else calculated here (on the calling
    // thread). `picture` itself when no effect is picked or its Mezcla is 0.
    QImage applyTo(const QImage &picture) const;

signals:
    void changed();       // another effect picked (or none)
    void valuesChanged(); // sliders, Mezcla or the text
    void busyChanged();
    void activeChanged(); // an effect started or stopped being previewed: "hasEdits" counts it
    void pictureReady(const QImage &picture); // the preview to put on the canvas

private:
    void schedule();
    void startJob(int stage);
    void finishJob(int generation, int stage, const QImage &image, bool wasCancelled);
    QImage proxyPicture() const;

    BaseProvider m_base;
    QString m_id;
    core::edit::EffectValues m_values{};
    double m_mix = 1.0;
    QString m_text;
    int m_generation = 0;
    int m_jobsRunning = 0;
    std::shared_ptr<std::atomic<bool>> m_cancel;
    // The calculations run here (not on the application-wide pool), so the destructor can wait for exactly these.
    QThreadPool m_pool;
    QTimer m_debounce;        // single-shot: starts the full-size calculation once the sliders rest
    QImage m_full;            // the exact full-size result for the current generation...
    bool m_fullValid = false; // ...once it has been calculated
    bool m_shown = false;     // a preview (not the real picture) is on the canvas
    mutable QImage m_proxy;   // the base shrunk for the quick preview, cached
};
