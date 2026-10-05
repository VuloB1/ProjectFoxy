#include "LensController.h"

#include "lens/LensDatabase.h"

#include <QMetaObject>
#include <QRunnable>
#include <QThreadPool>
#include <cmath>

using core::lens::Camera;
using core::lens::Correction;
using core::lens::Lens;
using core::lens::LensDatabase;


LensController::LensController(QObject *parent) : QObject(parent) {}

void LensController::load()
{
    if (m_started)
        return;
    m_started = true;
    QThreadPool::globalInstance()->start(QRunnable::create([this]() {
        LensDatabase::instance().ensureLoaded();
        QMetaObject::invokeMethod(this, [this]() {
            m_ready = true;
            emit readyChanged();
        }, Qt::QueuedConnection);
    }));
}

QStringList LensController::makers() const
{
    return m_ready ? LensDatabase::instance().cameraMakers() : QStringList();
}

QStringList LensController::models(const QString &maker) const
{
    return m_ready ? LensDatabase::instance().cameraModels(maker) : QStringList();
}

QVariantList LensController::lenses(const QString &maker, const QString &model, const QString &filter, int limit) const
{
    QVariantList list;
    if (!m_ready)
        return list;
    const LensDatabase &db = LensDatabase::instance();
    const Camera *camera = db.findCamera(maker, model);
    const std::vector<int> found = db.lensesFor(camera ? camera->mount : QString(), filter);
    for (int index : found) {
        if (list.size() >= limit)
            break;
        const Lens &l = db.lenses()[size_t(index)];
        QVariantMap m;
        m.insert(QStringLiteral("index"), index);
        QString name = l.modelName;
        if (!l.makerName.isEmpty() && !name.startsWith(l.makerName, Qt::CaseInsensitive))
            name = l.makerName + QLatin1Char(' ') + name;
        m.insert(QStringLiteral("name"), name);
        list.append(m);
    }
    return list;
}

QVariantMap LensController::lensInfo(int index) const
{
    QVariantMap m;
    const LensDatabase &db = LensDatabase::instance();
    if (!m_ready || index < 0 || size_t(index) >= db.lenses().size())
        return m;
    const Lens &l = db.lenses()[size_t(index)];
    double lo, hi;
    l.focalRange(lo, hi);
    QString name = l.modelName;
    if (!l.makerName.isEmpty() && !name.startsWith(l.makerName, Qt::CaseInsensitive))
        name = l.makerName + QLatin1Char(' ') + name;
    m.insert(QStringLiteral("name"), name);
    m.insert(QStringLiteral("focalMin"), l.focalMin > 0 ? l.focalMin : lo);
    m.insert(QStringLiteral("focalMax"), l.focalMax > 0 ? l.focalMax : hi);
    m.insert(QStringLiteral("apertureMin"), l.apertureMin);
    m.insert(QStringLiteral("apertureMax"), l.apertureMax);
    m.insert(QStringLiteral("type"), l.type);
    m.insert(QStringLiteral("distortion"), l.hasDistortion());
    m.insert(QStringLiteral("tca"), l.hasTca());
    m.insert(QStringLiteral("vignetting"), l.hasVignetting());
    return m;
}

QVariantMap LensController::guess(const QVariantMap &hints) const
{
    QVariantMap out;
    out.insert(QStringLiteral("lensIndex"), -1);
    if (!m_ready)
        return out;
    const LensDatabase &db = LensDatabase::instance();
    const Camera *camera = db.guessCamera(hints.value(QStringLiteral("make")).toString(), hints.value(QStringLiteral("model")).toString());
    if (camera) {
        out.insert(QStringLiteral("maker"), camera->makerName);
        out.insert(QStringLiteral("model"), camera->variant.isEmpty() ? camera->modelName
                                                                       : camera->modelName + QStringLiteral(" (") + camera->variant + QLatin1Char(')'));
        out.insert(QStringLiteral("crop"), camera->crop);
    }
    const int lens = db.guessLens(hints.value(QStringLiteral("lens")).toString(), camera ? camera->mount : QString(),
                                  hints.value(QStringLiteral("focal")).toDouble());
    out.insert(QStringLiteral("lensIndex"), lens);
    return out;
}

QVariantMap LensController::compute(const QString &maker, const QString &model, int lensIndex, double focal, double aperture,
                                    double distance, bool distortion, bool tca, bool vignetting, bool autoScale,
                                    double strength) const
{
    QVariantMap out;
    QVariantList values;
    for (int i = 0; i < 16; ++i)
        values.append(0.0);
    const LensDatabase &db = LensDatabase::instance();
    if (!m_ready || lensIndex < 0 || size_t(lensIndex) >= db.lenses().size()) {
        out.insert(QStringLiteral("values"), values);
        return out;
    }
    const Camera *camera = db.findCamera(maker, model);
    const double crop = camera ? camera->crop : db.lenses()[size_t(lensIndex)].crop;
    const Correction c = db.correction(db.lenses()[size_t(lensIndex)], crop, focal, aperture, distance);

    const std::array<double, 16> v = core::lens::correctionValues(c, distortion, tca, vignetting, autoScale, strength);
    for (int i = 0; i < 16; ++i)
        values[i] = v[size_t(i)];

    out.insert(QStringLiteral("values"), values);
    out.insert(QStringLiteral("distortion"), c.hasDistortion);
    out.insert(QStringLiteral("tca"), c.hasTca);
    out.insert(QStringLiteral("vignetting"), c.hasVignetting);
    out.insert(QStringLiteral("note"), c.note);
    return out;
}

QVariantList LensController::manual(double distortion, double fringes, double vignette, bool autoScale) const
{
    QVariantList values;
    for (double v : core::lens::manualValues(distortion, fringes, vignette, autoScale))
        values.append(v);
    return values;
}
