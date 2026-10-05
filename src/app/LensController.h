#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

// What the "Corrección de lente" tool asks of the lens database (core/lens/LensDatabase.h): lists to
// choose a camera and a lens from, a guess from the picture's EXIF, and the numbers of a correction
// ready to be put into the "lens" effect. The database is read in the background the first time
// the tool is opened.
class LensController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)

public:
    explicit LensController(QObject *parent = nullptr);

    bool ready() const { return m_ready; }

    // Starts reading the database (once; later calls do nothing).
    Q_INVOKABLE void load();

    Q_INVOKABLE QStringList makers() const;
    Q_INVOKABLE QStringList models(const QString &maker) const;
    // The lenses that fit the camera (every lens when no camera is picked), whose name has all the
    // words of `filter`: [{index, name}], at most `limit`.
    Q_INVOKABLE QVariantList lenses(const QString &maker, const QString &model, const QString &filter, int limit = 400) const;
    // {name, focalMin, focalMax, apertureMin, apertureMax, type, distortion, tca, vignetting} of lens `index`.
    Q_INVOKABLE QVariantMap lensInfo(int index) const;
    // The camera and lens a picture's EXIF stands for: {maker, model, lensIndex (-1 when unknown), crop}.
    Q_INVOKABLE QVariantMap guess(const QVariantMap &hints) const;
    // The 16 values of the "lens" effect for the chosen camera / lens / settings, and what the profile
    // covers: {values, distortion, tca, vignetting, note}. `strength` is 0..100.
    Q_INVOKABLE QVariantMap compute(const QString &maker, const QString &model, int lensIndex, double focal, double aperture,
                                    double distance, bool distortion, bool tca, bool vignetting, bool autoScale,
                                    double strength) const;
    // The same values for hand-set sliders (each -100..100): barrel/pincushion, red/blue fringes, vignette.
    Q_INVOKABLE QVariantList manual(double distortion, double fringes, double vignette, bool autoScale) const;

signals:
    void readyChanged();

private:
    bool m_ready = false;
    bool m_started = false;
};
