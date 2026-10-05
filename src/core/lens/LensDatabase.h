#pragma once

#include <QString>
#include <QStringList>
#include <array>
#include <map>
#include <mutex>
#include <vector>

namespace core::lens {

// The cameras and lens calibrations of the Lensfun database (https://lensfun.github.io, data files
// under CC BY-SA 3.0 - see docs/LICENCIAS.md): the files are read as they are, so a newer copy of the
// database can simply replace the shipped one. This reads and looks things up; what a correction does
// to the pixels is in core/edit/EffectsLens.cpp.

struct Camera {
    QString maker, model;       // as written in the database
    QString makerName, modelName; // what to show (the English names when there are two)
    QString variant;
    QString mount;
    double crop = 1.0;          // crop factor of the sensor
};

enum DistModel { kDistNone = 0, kDistPoly3 = 1, kDistPoly5 = 2, kDistPtLens = 3 };
enum TcaModel { kTcaNone = 0, kTcaLinear = 1, kTcaPoly3 = 2 };

struct DistCalib {
    double focal = 0.0;
    int model = kDistNone;
    double a = 0.0, b = 0.0, c = 0.0; // ptlens a, b, c; poly3 / poly5: k1 in a, k2 in b
};
struct TcaCalib {
    double focal = 0.0;
    int model = kTcaNone;
    double vr = 1.0, vb = 1.0, cr = 0.0, cb = 0.0, br = 0.0, bb = 0.0; // linear: vr / vb are kr / kb
};
struct VigCalib {
    double focal = 0.0, aperture = 0.0, distance = 0.0;
    double k1 = 0.0, k2 = 0.0, k3 = 0.0;
};

// What was calibrated together on one sensor size.
struct CalibSet {
    double crop = 1.0;
    std::vector<DistCalib> dist;
    std::vector<TcaCalib> tca;
    std::vector<VigCalib> vig;
};

struct Lens {
    QString maker, model;
    QString makerName, modelName;
    QString type = QStringLiteral("rectilinear");
    QStringList mounts;
    double crop = 1.0;
    double aspect = 1.5;        // width / height of the sensor the calibrations were made on
    double focalMin = 0.0, focalMax = 0.0;
    double apertureMin = 0.0, apertureMax = 0.0;
    std::vector<CalibSet> sets;

    bool hasDistortion() const;
    bool hasTca() const;
    bool hasVignetting() const;
    // The shortest and longest focal length a calibration exists for (or the lens' own range when none).
    void focalRange(double &lo, double &hi) const;
};

// The numbers a correction needs, for one lens at one focal length / aperture / distance and one camera.
struct Correction {
    bool hasDistortion = false, hasTca = false, hasVignetting = false;
    int distModel = kDistNone;
    double d1 = 0.0, d2 = 0.0, d3 = 0.0;           // see DistCalib
    double vr = 1.0, vb = 1.0, cr = 0.0, cb = 0.0, br = 0.0, bb = 0.0; // TCA poly3
    double k1 = 0.0, k2 = 0.0, k3 = 0.0;           // vignetting
    // The calibration's crop factor over the camera's (one per kind of data: they may come from different
    // sensors): radii measured for one sensor are scaled by it. `aspect` is the sensor shape calibrated on.
    double scaleDist = 1.0, scaleTca = 1.0, scaleVig = 1.0;
    double aspect = 1.5;
    QString note;                                    // something the user should know ("calibrado en otro sensor")
};

// The 16 values of the "lens" effect (see core/edit/EffectsLens.cpp) for a Correction: only the kinds
// of correction asked for (and that the profile has) are filled in. `strength` is 0..100.
std::array<double, 16> correctionValues(const Correction &c, bool distortion, bool tca, bool vignetting, bool autoScale,
                                        double strength);
// The same for hand-set sliders (each -100..100): barrel / pincushion, red-blue fringes, vignette.
std::array<double, 16> manualValues(double distortion, double fringes, double vignette, bool autoScale);

class LensDatabase {
public:
    // The database shared by the whole program, loaded the first time it is asked for (which takes a
    // moment: a few megabytes of XML). Thread-safe.
    static LensDatabase &instance();

    // Reads one Lensfun XML file's bytes; returns false when it is not a lens database.
    bool loadXml(const QByteArray &xml);
    // Every *.xml of a directory (a normal folder or a Qt resource such as ":/lensfun"); the number of files read.
    int loadDirectory(const QString &path);
    // Reads the shipped copy once (see LensDatabase::instance()).
    void ensureLoaded();
    bool isLoaded() const { return !m_cameras.empty() || !m_lenses.empty(); }

    const std::vector<Camera> &cameras() const { return m_cameras; }
    const std::vector<Lens> &lenses() const { return m_lenses; }

    // Camera makers and the models of one maker, as they are shown.
    QStringList cameraMakers() const;
    QStringList cameraModels(const QString &makerName) const;
    const Camera *findCamera(const QString &makerName, const QString &modelName) const;
    // The camera that an EXIF Make/Model pair stands for (or nullptr).
    const Camera *guessCamera(const QString &exifMake, const QString &exifModel) const;

    // Indices into lenses() of the lenses that fit a camera's mount (all of them when `mount` is empty),
    // narrowed to the ones whose name contains every word of `filter`, ordered by name.
    std::vector<int> lensesFor(const QString &cameraMount, const QString &filter = QString()) const;
    // The best match for what a picture's EXIF says about its lens (-1 when nothing fits well).
    int guessLens(const QString &exifLens, const QString &cameraMount, double focal) const;

    // The correction numbers (interpolated like Lensfun does) for a lens used on a camera of `cameraCrop`.
    Correction correction(const Lens &lens, double cameraCrop, double focal, double aperture, double distance) const;

    // Mounts a lens of mount `lensMount` can be used on with a camera of mount `cameraMount`.
    bool mountsFit(const QString &cameraMount, const QString &lensMount) const;

private:
    std::vector<Camera> m_cameras;
    std::vector<Lens> m_lenses;
    std::map<QString, QStringList> m_compat; // mount -> the mounts it can also take
    std::map<QString, int> m_lensIndex;      // maker + "|" + model -> index in m_lenses
    std::once_flag m_loadOnce;
};

} // namespace core::lens
