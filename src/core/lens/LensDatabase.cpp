#include "LensDatabase.h"

#include <QDir>
#include <QFile>
#include <QXmlStreamReader>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <set>

namespace core::lens {

namespace {

QString normalized(const QString &text)
{
    QString out;
    out.reserve(text.size());
    for (const QChar &c : text)
        if (c.isLetterOrNumber())
            out.append(c.toLower());
    return out;
}

QStringList wordsOf(const QString &text)
{
    QStringList words;
    QString current;
    for (const QChar &c : text) {
        if (c.isLetterOrNumber()) {
            current.append(c.toLower());
        } else if (!current.isEmpty()) {
            words << current;
            current.clear();
        }
    }
    if (!current.isEmpty())
        words << current;
    return words;
}

// Cubic Hermite spline through y2 and y3 (t in 0..1) with Catmull-Rom tangents; FLT_MAX marks a missing neighbour.
double splineInterpolate(double y1, double y2, double y3, double y4, double t)
{
    const double tg2 = y1 == FLT_MAX ? y3 - y2 : (y3 - y1) * 0.5;
    const double tg3 = y4 == FLT_MAX ? y3 - y2 : (y4 - y2) * 0.5;
    const double t2 = t * t, t3 = t2 * t;
    return (2 * t3 - 3 * t2 + 1) * y2 + (t3 - 2 * t2 + t) * tg2 + (-2 * t3 + 3 * t2) * y3 + (t3 - t2) * tg3;
}

// The (up to) four calibration points of one kind around `focal`: two below it and two above, or the exact one.
template <class T>
struct Neighbours {
    const T *below1 = nullptr, *below2 = nullptr; // nearest, second nearest below
    const T *above1 = nullptr, *above2 = nullptr;
    const T *exact = nullptr;
};

template <class T>
Neighbours<T> neighboursOf(const std::vector<T> &points, double focal)
{
    std::vector<const T *> sorted;
    sorted.reserve(points.size());
    for (const T &p : points)
        sorted.push_back(&p);
    std::stable_sort(sorted.begin(), sorted.end(), [](const T *x, const T *y) { return x->focal < y->focal; });
    Neighbours<T> n;
    int below = -1; // the last point below `focal`
    for (int i = 0; i < int(sorted.size()); ++i) {
        if (sorted[size_t(i)]->focal == focal) {
            n.exact = sorted[size_t(i)];
            return n;
        }
        if (sorted[size_t(i)]->focal < focal)
            below = i;
    }
    if (below >= 0)
        n.below1 = sorted[size_t(below)];
    if (below >= 1)
        n.below2 = sorted[size_t(below - 1)];
    if (below + 1 < int(sorted.size()))
        n.above1 = sorted[size_t(below + 1)];
    if (below + 2 < int(sorted.size()))
        n.above2 = sorted[size_t(below + 2)];
    return n;
}

double attr(const QXmlStreamAttributes &a, const char *name, double def = 0.0)
{
    const QStringView v = a.value(QLatin1String(name));
    if (v.isEmpty())
        return def;
    bool ok = false;
    const double d = v.toString().toDouble(&ok);
    return ok ? d : def;
}

} // namespace

bool Lens::hasDistortion() const
{
    for (const CalibSet &s : sets)
        if (!s.dist.empty())
            return true;
    return false;
}
bool Lens::hasTca() const
{
    for (const CalibSet &s : sets)
        if (!s.tca.empty())
            return true;
    return false;
}
bool Lens::hasVignetting() const
{
    for (const CalibSet &s : sets)
        if (!s.vig.empty())
            return true;
    return false;
}
void Lens::focalRange(double &lo, double &hi) const
{
    lo = FLT_MAX;
    hi = 0.0;
    for (const CalibSet &s : sets) {
        for (const DistCalib &d : s.dist) { lo = std::min(lo, d.focal); hi = std::max(hi, d.focal); }
        for (const TcaCalib &t : s.tca) { lo = std::min(lo, t.focal); hi = std::max(hi, t.focal); }
        for (const VigCalib &v : s.vig) { lo = std::min(lo, v.focal); hi = std::max(hi, v.focal); }
    }
    if (hi <= 0.0) {
        lo = focalMin;
        hi = focalMax;
    }
    if (lo <= 0.0 || lo == FLT_MAX)
        lo = focalMin > 0 ? focalMin : 1.0;
    if (hi < lo)
        hi = lo;
}

LensDatabase &LensDatabase::instance()
{
    static LensDatabase db;
    return db;
}

void LensDatabase::ensureLoaded()
{
    std::call_once(m_loadOnce, [this] { loadDirectory(QStringLiteral(":/lensfun")); });
}

int LensDatabase::loadDirectory(const QString &path)
{
    int count = 0;
    const QDir dir(path);
    const QStringList files = dir.entryList({QStringLiteral("*.xml")}, QDir::Files, QDir::Name);
    for (const QString &name : files) {
        QFile f(dir.filePath(name));
        if (f.open(QIODevice::ReadOnly) && loadXml(f.readAll()))
            ++count;
    }
    return count;
}

bool LensDatabase::loadXml(const QByteArray &xml)
{
    QXmlStreamReader r(xml);
    bool sawRoot = false;

    auto readText = [&r]() { return r.readElementText(QXmlStreamReader::SkipChildElements).trimmed(); };

    while (!r.atEnd()) {
        r.readNext();
        if (!r.isStartElement())
            continue;
        const QString tag = r.name().toString();
        if (tag == QLatin1String("lensdatabase")) {
            sawRoot = true;
        } else if (tag == QLatin1String("mount")) {
            QString name;
            QStringList compat;
            while (!(r.isEndElement() && r.name() == QLatin1String("mount")) && !r.atEnd()) {
                r.readNext();
                if (r.isStartElement()) {
                    if (r.name() == QLatin1String("name") && r.attributes().value(QLatin1String("lang")).isEmpty())
                        name = readText();
                    else if (r.name() == QLatin1String("compat"))
                        compat << readText();
                }
            }
            if (!name.isEmpty())
                m_compat[name] += compat;
        } else if (tag == QLatin1String("camera")) {
            Camera c;
            while (!(r.isEndElement() && r.name() == QLatin1String("camera")) && !r.atEnd()) {
                r.readNext();
                if (!r.isStartElement())
                    continue;
                const QString t = r.name().toString();
                const bool english = r.attributes().value(QLatin1String("lang")) == QLatin1String("en");
                const bool other = !english && !r.attributes().value(QLatin1String("lang")).isEmpty();
                if (t == QLatin1String("maker")) {
                    const QString v = readText();
                    if (english) c.makerName = v;
                    else if (!other && c.maker.isEmpty()) c.maker = v;
                } else if (t == QLatin1String("model")) {
                    const QString v = readText();
                    if (english) c.modelName = v;
                    else if (!other && c.model.isEmpty()) c.model = v;
                } else if (t == QLatin1String("variant")) {
                    c.variant = readText();
                } else if (t == QLatin1String("mount")) {
                    c.mount = readText();
                } else if (t == QLatin1String("cropfactor")) {
                    c.crop = readText().toDouble();
                }
            }
            if (c.makerName.isEmpty()) c.makerName = c.maker;
            if (c.modelName.isEmpty()) c.modelName = c.model;
            if (!c.model.isEmpty() && c.crop > 0)
                m_cameras.push_back(std::move(c));
        } else if (tag == QLatin1String("lens")) {
            Lens l;
            CalibSet set;
            bool sawCalibration = false;
            while (!(r.isEndElement() && r.name() == QLatin1String("lens")) && !r.atEnd()) {
                r.readNext();
                if (!r.isStartElement())
                    continue;
                const QString t = r.name().toString();
                const QXmlStreamAttributes a = r.attributes();
                const bool english = a.value(QLatin1String("lang")) == QLatin1String("en");
                const bool other = !english && !a.value(QLatin1String("lang")).isEmpty();
                if (t == QLatin1String("maker")) {
                    const QString v = readText();
                    if (english) l.makerName = v;
                    else if (!other && l.maker.isEmpty()) l.maker = v;
                } else if (t == QLatin1String("model")) {
                    const QString v = readText();
                    if (english) l.modelName = v;
                    else if (!other && l.model.isEmpty()) l.model = v;
                } else if (t == QLatin1String("mount")) {
                    l.mounts << readText();
                } else if (t == QLatin1String("type")) {
                    l.type = readText();
                } else if (t == QLatin1String("cropfactor")) {
                    l.crop = readText().toDouble();
                } else if (t == QLatin1String("aspect-ratio")) {
                    const QString v = readText();
                    const int slash = v.indexOf(QLatin1Char(':'));
                    double ratio = v.toDouble();
                    if (slash > 0)
                        ratio = v.left(slash).toDouble() / std::max(1.0, v.mid(slash + 1).toDouble());
                    if (ratio > 0.1)
                        l.aspect = ratio;
                } else if (t == QLatin1String("focal")) {
                    if (a.hasAttribute(QLatin1String("value")))
                        l.focalMin = l.focalMax = attr(a, "value");
                    else {
                        l.focalMin = attr(a, "min");
                        l.focalMax = attr(a, "max", l.focalMin);
                    }
                } else if (t == QLatin1String("aperture")) {
                    if (a.hasAttribute(QLatin1String("value")))
                        l.apertureMin = l.apertureMax = attr(a, "value");
                    else {
                        l.apertureMin = attr(a, "min");
                        l.apertureMax = attr(a, "max", l.apertureMin);
                    }
                } else if (t == QLatin1String("calibration")) {
                    sawCalibration = true;
                } else if (t == QLatin1String("distortion")) {
                    DistCalib d;
                    const QString model = a.value(QLatin1String("model")).toString();
                    d.focal = attr(a, "focal");
                    if (model == QLatin1String("poly3")) {
                        d.model = kDistPoly3;
                        d.a = attr(a, "k1");
                    } else if (model == QLatin1String("poly5")) {
                        d.model = kDistPoly5;
                        d.a = attr(a, "k1");
                        d.b = attr(a, "k2");
                    } else if (model == QLatin1String("ptlens")) {
                        d.model = kDistPtLens;
                        d.a = attr(a, "a");
                        d.b = attr(a, "b");
                        d.c = attr(a, "c");
                    }
                    if (d.model != kDistNone)
                        set.dist.push_back(d);
                } else if (t == QLatin1String("tca")) {
                    TcaCalib c;
                    const QString model = a.value(QLatin1String("model")).toString();
                    c.focal = attr(a, "focal");
                    if (model == QLatin1String("linear")) {
                        c.model = kTcaLinear;
                        c.vr = attr(a, "kr", 1.0);
                        c.vb = attr(a, "kb", 1.0);
                    } else if (model == QLatin1String("poly3")) {
                        c.model = kTcaPoly3;
                        c.vr = attr(a, "vr", 1.0);
                        c.vb = attr(a, "vb", 1.0);
                        c.cr = attr(a, "cr");
                        c.cb = attr(a, "cb");
                        c.br = attr(a, "br");
                        c.bb = attr(a, "bb");
                    }
                    if (c.model != kTcaNone)
                        set.tca.push_back(c);
                } else if (t == QLatin1String("vignetting")) {
                    if (a.value(QLatin1String("model")) == QLatin1String("pa")) {
                        VigCalib v;
                        v.focal = attr(a, "focal");
                        v.aperture = attr(a, "aperture");
                        v.distance = attr(a, "distance", 1000.0);
                        v.k1 = attr(a, "k1");
                        v.k2 = attr(a, "k2");
                        v.k3 = attr(a, "k3");
                        set.vig.push_back(v);
                    }
                }
            }
            (void)sawCalibration;
            if (l.makerName.isEmpty()) l.makerName = l.maker;
            if (l.modelName.isEmpty()) l.modelName = l.model;
            if (l.model.isEmpty() || l.mounts.isEmpty() || l.crop <= 0)
                continue;
            set.crop = l.crop;
            // The same lens may be listed several times (once per sensor size it was calibrated on): they are one lens.
            const QString key = l.maker + QLatin1Char('|') + l.model;
            const auto it = m_lensIndex.find(key);
            if (it == m_lensIndex.end()) {
                if (!set.dist.empty() || !set.tca.empty() || !set.vig.empty())
                    l.sets.push_back(std::move(set));
                m_lensIndex[key] = int(m_lenses.size());
                m_lenses.push_back(std::move(l));
            } else {
                Lens &known = m_lenses[size_t(it->second)];
                for (const QString &m : l.mounts)
                    if (!known.mounts.contains(m))
                        known.mounts << m;
                if (!set.dist.empty() || !set.tca.empty() || !set.vig.empty())
                    known.sets.push_back(std::move(set));
                if (known.focalMax <= 0 && l.focalMax > 0) {
                    known.focalMin = l.focalMin;
                    known.focalMax = l.focalMax;
                }
            }
        }
    }
    return sawRoot && !r.hasError();
}

QStringList LensDatabase::cameraMakers() const
{
    std::set<QString> makers;
    for (const Camera &c : m_cameras)
        makers.insert(c.makerName);
    return QStringList(makers.begin(), makers.end());
}

QStringList LensDatabase::cameraModels(const QString &makerName) const
{
    std::set<QString> models;
    for (const Camera &c : m_cameras)
        if (c.makerName == makerName)
            models.insert(c.variant.isEmpty() ? c.modelName : c.modelName + QStringLiteral(" (") + c.variant + QLatin1Char(')'));
    return QStringList(models.begin(), models.end());
}

const Camera *LensDatabase::findCamera(const QString &makerName, const QString &modelName) const
{
    for (const Camera &c : m_cameras) {
        const QString shown = c.variant.isEmpty() ? c.modelName : c.modelName + QStringLiteral(" (") + c.variant + QLatin1Char(')');
        if (c.makerName == makerName && shown == modelName)
            return &c;
    }
    return nullptr;
}

const Camera *LensDatabase::guessCamera(const QString &exifMake, const QString &exifModel) const
{
    const QString mk = normalized(exifMake), md = normalized(exifModel);
    if (md.isEmpty())
        return nullptr;
    const Camera *best = nullptr;
    int bestScore = 0;
    for (const Camera &c : m_cameras) {
        const QString m1 = normalized(c.model), m2 = normalized(c.modelName);
        const QString k1 = normalized(c.maker), k2 = normalized(c.makerName);
        int score = 0;
        if (md == m1 || md == m2)
            score = 100;
        else if (!mk.isEmpty() && (md == k1 + m2 || md == k2 + m2 || md == k1 + m1))
            score = 95;
        else if (!mk.isEmpty() && (mk.contains(k2) || k1.contains(mk) || mk.contains(k1)) && (md.endsWith(m2) || m2.endsWith(md)))
            score = 60 + int(std::min<qsizetype>(m2.size(), 20));
        if (score > bestScore) {
            bestScore = score;
            best = &c;
        }
    }
    return bestScore >= 60 ? best : nullptr;
}

bool LensDatabase::mountsFit(const QString &cameraMount, const QString &lensMount) const
{
    if (cameraMount.isEmpty() || cameraMount == lensMount)
        return true;
    const auto it = m_compat.find(cameraMount);
    if (it != m_compat.end() && it->second.contains(lensMount))
        return true;
    const auto jt = m_compat.find(lensMount);
    return jt != m_compat.end() && jt->second.contains(cameraMount) && lensMount != QLatin1String("Generic");
}

std::vector<int> LensDatabase::lensesFor(const QString &cameraMount, const QString &filter) const
{
    QStringList words;
    for (const QString &w : wordsOf(filter))
        words << w;
    std::vector<int> result;
    for (size_t i = 0; i < m_lenses.size(); ++i) {
        const Lens &l = m_lenses[i];
        bool fits = cameraMount.isEmpty();
        for (const QString &m : l.mounts)
            fits |= mountsFit(cameraMount, m);
        if (!fits)
            continue;
        if (!words.isEmpty()) {
            const QString hay = normalized(l.makerName + QLatin1Char(' ') + l.modelName + QLatin1Char(' ') + l.model);
            bool all = true;
            for (const QString &w : words)
                all &= hay.contains(w);
            if (!all)
                continue;
        }
        result.push_back(int(i));
    }
    std::sort(result.begin(), result.end(), [this](int a, int b) {
        const Lens &x = m_lenses[size_t(a)], &y = m_lenses[size_t(b)];
        if (x.makerName != y.makerName)
            return x.makerName.compare(y.makerName, Qt::CaseInsensitive) < 0;
        return x.modelName.compare(y.modelName, Qt::CaseInsensitive) < 0;
    });
    return result;
}

int LensDatabase::guessLens(const QString &exifLens, const QString &cameraMount, double focal) const
{
    const QStringList wanted = wordsOf(exifLens);
    if (wanted.isEmpty())
        return -1;
    static const std::set<QString> noise = {QStringLiteral("lens"), QStringLiteral("mm"), QStringLiteral("f"), QStringLiteral("af"),
                                            QStringLiteral("objetivo")};
    std::set<QString> want;
    for (const QString &w : wanted)
        if (!noise.count(w))
            want.insert(w);
    if (want.empty())
        return -1;
    const QString wantNorm = normalized(exifLens);
    int best = -1;
    double bestScore = 0.0;
    for (size_t i = 0; i < m_lenses.size(); ++i) {
        const Lens &l = m_lenses[i];
        bool fits = cameraMount.isEmpty();
        for (const QString &m : l.mounts)
            fits |= mountsFit(cameraMount, m);
        if (!fits)
            continue;
        std::set<QString> have;
        for (const QString &w : wordsOf(l.makerName + QLatin1Char(' ') + l.modelName))
            if (!noise.count(w))
                have.insert(w);
        if (have.empty())
            continue;
        int common = 0;
        for (const QString &w : want)
            common += have.count(w) ? 1 : 0;
        double score = double(common) / double(want.size() + have.size() - common);
        if (normalized(l.makerName + l.modelName) == wantNorm || normalized(l.modelName) == wantNorm)
            score += 1.0;
        if (focal > 0 && l.focalMax > 0 && focal >= l.focalMin - 0.5 && focal <= l.focalMax + 0.5)
            score += 0.1;
        if (score > bestScore) {
            bestScore = score;
            best = int(i);
        }
    }
    return bestScore >= 0.5 ? best : -1;
}

Correction LensDatabase::correction(const Lens &lens, double cameraCrop, double focal, double aperture, double distance) const
{
    Correction out;
    if (cameraCrop <= 0)
        cameraCrop = 1.0;
    if (distance <= 0)
        distance = 1000.0;
    if (aperture <= 0)
        aperture = 5.6;

    // For each kind of data, the calibration set whose sensor is the closest one that is not smaller than the camera's
    // (Lensfun's rule: crop ratio >= 0.96), else the nearest one at all.
    auto pickSet = [&](auto hasData, bool &other) -> const CalibSet * {
        const CalibSet *best = nullptr;
        double bestRatio = 1e6;
        for (const CalibSet &s : lens.sets) {
            if (!hasData(s))
                continue;
            const double r = cameraCrop / s.crop;
            if (r >= 0.96 && r < bestRatio) {
                bestRatio = r;
                best = &s;
            }
        }
        other = false;
        if (!best) {
            double bestLog = 1e9;
            for (const CalibSet &s : lens.sets) {
                if (!hasData(s))
                    continue;
                const double lg = std::abs(std::log(cameraCrop / s.crop));
                if (lg < bestLog) {
                    bestLog = lg;
                    best = &s;
                }
            }
            other = best != nullptr;
        }
        return best;
    };

    bool other = false;
    // --- distortion
    if (const CalibSet *set = pickSet([](const CalibSet &s) { return !s.dist.empty(); }, other)) {
        const int model = set->dist.front().model;
        std::vector<DistCalib> pts;
        for (const DistCalib &d : set->dist)
            if (d.model == model)
                pts.push_back(d);
        const Neighbours<DistCalib> n = neighboursOf(pts, focal);
        double terms[3] = {0, 0, 0};
        bool ok = true;
        if (n.exact) {
            terms[0] = n.exact->a; terms[1] = n.exact->b; terms[2] = n.exact->c;
        } else if (!n.below1 || !n.above1) {
            const DistCalib *one = n.below1 ? n.below1 : n.above1;
            if (one) { terms[0] = one->a; terms[1] = one->b; terms[2] = one->c; } else ok = false;
        } else {
            const double t = (focal - n.below1->focal) / (n.above1->focal - n.below1->focal);
            // the terms follow a 1/f law: interpolate focal * term, divide by the wanted focal afterwards
            auto term = [&](double DistCalib::*member) {
                return splineInterpolate(n.below2 ? n.below2->*member * n.below2->focal : FLT_MAX, n.below1->*member * n.below1->focal,
                                         n.above1->*member * n.above1->focal, n.above2 ? n.above2->*member * n.above2->focal : FLT_MAX, t)
                       / focal;
            };
            terms[0] = term(&DistCalib::a);
            terms[1] = term(&DistCalib::b);
            terms[2] = term(&DistCalib::c);
        }
        if (ok) {
            out.hasDistortion = true;
            out.distModel = model;
            out.d1 = terms[0]; out.d2 = terms[1]; out.d3 = terms[2];
            out.scaleDist = set->crop / cameraCrop;
            out.aspect = lens.aspect;
            if (other)
                out.note = QStringLiteral("La distorsión de esta lente está calibrada en un sensor de otro tamaño.");
        }
    }
    // --- chromatic aberration
    if (const CalibSet *set = pickSet([](const CalibSet &s) { return !s.tca.empty(); }, other)) {
        const int model = set->tca.front().model;
        std::vector<TcaCalib> pts;
        for (const TcaCalib &c : set->tca)
            if (c.model == model)
                pts.push_back(c);
        const Neighbours<TcaCalib> n = neighboursOf(pts, focal);
        TcaCalib r;
        bool ok = true;
        if (n.exact) {
            r = *n.exact;
        } else if (!n.below1 || !n.above1) {
            const TcaCalib *one = n.below1 ? n.below1 : n.above1;
            if (one) r = *one; else ok = false;
        } else {
            const double t = (focal - n.below1->focal) / (n.above1->focal - n.below1->focal);
            // vr and vb stay as they are; the other terms follow the 1/f law
            auto term = [&](double TcaCalib::*member, bool scaled) {
                const double s1 = scaled ? n.below1->focal : 1.0, s2 = scaled ? n.above1->focal : 1.0;
                const double s0 = n.below2 && scaled ? n.below2->focal : 1.0, s3 = n.above2 && scaled ? n.above2->focal : 1.0;
                return splineInterpolate(n.below2 ? n.below2->*member * s0 : FLT_MAX, n.below1->*member * s1, n.above1->*member * s2,
                                         n.above2 ? n.above2->*member * s3 : FLT_MAX, t) / (scaled ? focal : 1.0);
            };
            r.model = model;
            r.vr = term(&TcaCalib::vr, false);
            r.vb = term(&TcaCalib::vb, false);
            r.cr = term(&TcaCalib::cr, true);
            r.cb = term(&TcaCalib::cb, true);
            r.br = term(&TcaCalib::br, true);
            r.bb = term(&TcaCalib::bb, true);
        }
        if (ok) {
            out.hasTca = true;
            out.vr = r.vr; out.vb = r.vb; out.cr = r.cr; out.cb = r.cb; out.br = r.br; out.bb = r.bb;
            out.scaleTca = set->crop / cameraCrop;
            if (!out.hasDistortion)
                out.aspect = lens.aspect;
        }
    }
    // --- vignetting: inverse-distance weighting over focal length, aperture and distance
    if (const CalibSet *set = pickSet([](const CalibSet &s) { return !s.vig.empty(); }, other)) {
        double lo = lens.focalMin, hi = lens.focalMax;
        if (hi <= lo) {
            lo = FLT_MAX;
            hi = 0;
            for (const VigCalib &v : set->vig) { lo = std::min(lo, v.focal); hi = std::max(hi, v.focal); }
        }
        const double df = hi - lo;
        double totalWeight = 0, smallest = FLT_MAX;
        double k[3] = {0, 0, 0};
        bool exact = false;
        for (const VigCalib &v : set->vig) {
            double f1 = focal - lo, f2 = v.focal - lo;
            if (df != 0) { f1 /= df; f2 /= df; }
            const double a1 = 4.0 / aperture, a2 = 4.0 / (v.aperture > 0 ? v.aperture : 1.0);
            const double d1 = 0.1 / distance, d2 = 0.1 / (v.distance > 0 ? v.distance : 1000.0);
            const double dist = std::sqrt((f2 - f1) * (f2 - f1) + (a2 - a1) * (a2 - a1) + (d2 - d1) * (d2 - d1));
            if (dist < 0.0001) {
                k[0] = v.k1; k[1] = v.k2; k[2] = v.k3;
                exact = true;
                smallest = 0;
                break;
            }
            smallest = std::min(smallest, dist);
            const double w = std::abs(1.0 / std::pow(dist, 3.5));
            k[0] += w * v.k1; k[1] += w * v.k2; k[2] += w * v.k3;
            totalWeight += w;
        }
        if (!exact && totalWeight > 0) {
            for (double &x : k)
                x /= totalWeight;
        }
        if (smallest <= 1.0 && (exact || totalWeight > 0)) {
            out.hasVignetting = true;
            out.k1 = k[0]; out.k2 = k[1]; out.k3 = k[2];
            out.scaleVig = set->crop / cameraCrop;
            if (!out.hasDistortion && !out.hasTca)
                out.aspect = lens.aspect;
        }
    }
    return out;
}

std::array<double, 16> correctionValues(const Correction &c, bool distortion, bool tca, bool vignetting, bool autoScale,
                                        double strength)
{
    std::array<double, 16> v{};
    // The effect works in one set of units (those of the distortion data); the other terms are put into them.
    const double dist = c.scaleDist > 0 ? c.scaleDist : 1.0;
    const double sigmaT = (c.hasTca ? c.scaleTca : dist) / dist;
    const double hug = std::hypot(c.aspect, 1.0);
    // r in Hugin units = r_px / (half the diagonal) * kr, with kr = sensor-shape factor * crop ratio
    const double kr = dist * hug;
    // the vignetting terms are written for r = 1 at the corner: r_v = r_hug * (scaleVig / kr)
    const double rhoV = (c.hasVignetting ? c.scaleVig : kr) / kr;

    const bool useDist = distortion && c.hasDistortion;
    v[0] = useDist ? c.distModel : 0;
    v[1] = useDist ? c.d1 : 0.0;
    v[2] = useDist ? c.d2 : 0.0;
    v[3] = useDist ? c.d3 : 0.0;
    const bool useTca = tca && c.hasTca;
    v[4] = useTca ? c.vr : 1.0;
    v[5] = useTca ? c.vb : 1.0;
    v[6] = useTca ? c.cr * sigmaT : 0.0;
    v[7] = useTca ? c.cb * sigmaT : 0.0;
    v[8] = useTca ? c.br * sigmaT * sigmaT : 0.0;
    v[9] = useTca ? c.bb * sigmaT * sigmaT : 0.0;
    const bool useVig = vignetting && c.hasVignetting;
    v[10] = useVig ? c.k1 * rhoV * rhoV : 0.0;
    v[11] = useVig ? c.k2 * std::pow(rhoV, 4.0) : 0.0;
    v[12] = useVig ? c.k3 * std::pow(rhoV, 6.0) : 0.0;
    v[13] = kr;
    v[14] = autoScale && useDist ? 1.0 : 0.0;
    v[15] = strength;
    return v;
}

std::array<double, 16> manualValues(double distortion, double fringes, double vignette, bool autoScale)
{
    std::array<double, 16> v{};
    // Barrel / pincushion as a one-term (poly3) correction; positive straightens a barrel (edges stretched out).
    v[0] = distortion != 0.0 ? 1 : 0;
    v[1] = -0.04 * distortion / 100.0; // at 100 the corners move by about 9%
    v[4] = 1.0 + 0.004 * fringes / 100.0; // red scaled one way, blue the other
    v[5] = 1.0 - 0.004 * fringes / 100.0;
    v[10] = -0.155 * vignette / 100.0;    // brightens (positive) / darkens the corners: x2 at the corner at 100
    v[13] = 1.8;                          // r = 1 at about half the short side of a 3:2 picture (1.8 at the corner)
    // either sign leaves something empty: a barrel correction the middle of the edges, a pincushion one the corners
    v[14] = autoScale && distortion != 0.0 ? 1.0 : 0.0;
    v[15] = 100.0;
    return v;
}

} // namespace core::lens
