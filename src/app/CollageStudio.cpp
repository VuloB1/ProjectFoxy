#include "CollageStudio.h"
#include "PaneImageProvider.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageWriter>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QMutexLocker>
#include <QPainter>
#include <QRunnable>
#include <QSaveFile>
#include <QStandardPaths>
#include <QThreadPool>
#include <algorithm>
#include <cmath>
#include <random>
#include <webp/encode.h>
#include "Translate.h"

using namespace core::collage;

namespace {

constexpr int kMaxCells = 12;
constexpr double kMinCell = 0.05;

QVariantMap defaultOptions()
{
    QVariantMap m;
    m.insert("sizeW", 2000);
    m.insert("sizeH", 2000);
    m.insert("background", 0);
    m.insert("color1", 0xFFFFFF);
    m.insert("color2", 0xBEC3D2);
    m.insert("gradientAngle", 90);
    m.insert("photoBlur", 8.0);
    m.insert("transparentBackground", false);
    m.insert("margin", 3.0);
    m.insert("spacing", 2.0);
    m.insert("radius", 0.0);
    m.insert("borderWidth", 0.0);
    m.insert("borderColor", 0xFFFFFF);
    m.insert("shadow", 0.0);
    m.insert("shadowBlur", 1.5);
    m.insert("shadowOffset", 0.6);
    m.insert("cellFillOn", false);
    m.insert("cellFill", 0x303030);
    m.insert("free", false);
    m.insert("format", 0);
    m.insert("quality", 92);
    return m;
}

QColor rgb(const QVariant &v, int alpha = 255)
{
    const uint c = v.toUInt() & 0xFFFFFF;
    return QColor(int((c >> 16) & 255), int((c >> 8) & 255), int(c & 255), alpha);
}

// A cell for the free layout: where the next one goes (a cascade, so they do not hide each other).
QRectF freeRect(int index)
{
    const double off = 0.06 * (index % 6);
    return QRectF(0.08 + off, 0.08 + off, 0.42, 0.42);
}

} // namespace

CollageStudio::CollageStudio(PaneImageStore *store, QObject *parent) : QObject(parent), m_store(store), m_options(defaultOptions())
{
    m_cells.resize(4);
    const auto layouts = layoutsFor(4);
    for (size_t i = 0; i < 4; ++i)
        m_cells[i].rect = layouts.front().rects[i];
    rebuildSnapshot();
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!dir.isEmpty())
        setTemplatesFile(dir + QStringLiteral("/collage-templates.json"));
}

CollageStudio::~CollageStudio()
{
    if (m_cancel)
        m_cancel->store(true);
    QThreadPool::globalInstance()->waitForDone(3000);
}

// ---- reading -----------------------------------------------------------------------------------------------------------

Style CollageStudio::styleFromOptions() const
{
    const QVariantMap &o = m_options;
    Style s;
    s.size = QSize(o.value("sizeW").toInt(), o.value("sizeH").toInt());
    s.background = BackgroundMode(std::clamp(o.value("background").toInt(), 0, 2));
    s.color1 = rgb(o.value("color1"), o.value("transparentBackground").toBool() ? 0 : 255);
    s.color2 = rgb(o.value("color2"));
    s.gradientAngle = o.value("gradientAngle").toDouble();
    s.photoBlur = o.value("photoBlur").toDouble();
    s.margin = o.value("margin").toDouble();
    s.spacing = o.value("spacing").toDouble();
    s.radius = o.value("radius").toDouble();
    s.borderWidth = o.value("borderWidth").toDouble();
    s.borderColor = rgb(o.value("borderColor"));
    s.shadow = o.value("shadow").toDouble();
    s.shadowBlur = o.value("shadowBlur").toDouble();
    s.shadowOffset = o.value("shadowOffset").toDouble();
    s.cellFill = o.value("cellFillOn").toBool() ? rgb(o.value("cellFill")) : QColor(0, 0, 0, 0);
    return s;
}

std::shared_ptr<const CollageStudio::Snapshot> CollageStudio::snapshot() const
{
    QMutexLocker lock(&m_mutex);
    return m_snapshot;
}

void CollageStudio::rebuildSnapshot()
{
    auto snap = std::make_shared<Snapshot>();
    snap->cells = m_cells;
    snap->style = styleFromOptions();
    QMutexLocker lock(&m_mutex);
    m_snapshot = std::move(snap);
}

void CollageStudio::touch(bool layout)
{
    ++m_revision;
    rebuildSnapshot();
    emit changed();
    if (layout)
        emit layoutChanged();
}

QSize CollageStudio::canvasSize() const { return snapshot()->style.size; }

QRectF CollageStudio::canvasRectOf(const Cell &cell) const
{
    const auto snap = snapshot();
    const QSize canvas = snap->style.size;
    const QRectF px = cellPixelRect(cell.rect, snap->style, canvas);
    return QRectF(px.x() / canvas.width(), px.y() / canvas.height(), px.width() / canvas.width(), px.height() / canvas.height());
}

QPointF CollageStudio::toLayoutDelta(double dx, double dy) const
{
    const auto snap = snapshot();
    const QSize canvas = snap->style.size;
    const QRectF area = layoutAreaPx(snap->style, canvas);
    return QPointF(dx * canvas.width() / area.width(), dy * canvas.height() / area.height());
}

QString CollageStudio::pathFrom(const QString &pathOrUrl)
{
    return pathOrUrl.startsWith(QLatin1String("file:")) ? QUrl(pathOrUrl).toLocalFile() : pathOrUrl;
}

QVariantList CollageStudio::presets() const
{
    QVariantList list;
    for (const Layout &l : layoutsFor(cellCount())) {
        QVariantList rects;
        for (const QRectF &r : l.rects)
            rects.append(QVariant(QVariantList{r.x(), r.y(), r.width(), r.height()})); // (a bare list would be spliced in)
        list.append(QVariantMap{{"name", core::tr(l.name)}, {"rects", rects}});
    }
    return list;
}

QVariantList CollageStudio::cells() const
{
    QVariantList list;
    for (const Cell &c : m_cells) {
        const QRectF r = canvasRectOf(c);
        QVariantMap m;
        m.insert("x", r.x());
        m.insert("y", r.y());
        m.insert("w", r.width());
        m.insert("h", r.height());
        m.insert("path", c.content.path);
        m.insert("name", QFileInfo(c.content.path).fileName());
        m.insert("empty", c.content.path.isEmpty());
        m.insert("zoom", c.content.zoom);
        m.insert("panX", c.content.panX);
        m.insert("panY", c.content.panY);
        m.insert("rotation", c.content.rotation);
        m.insert("flipH", c.content.flipH);
        m.insert("flipV", c.content.flipV);
        m.insert("overflow", c.content.overflow);
        list.append(m);
    }
    return list;
}

QVariantList CollageStudio::dividers() const
{
    QVariantList list;
    if (m_options.value("free").toBool())
        return list;
    const auto snap = snapshot();
    const QSize canvas = snap->style.size;
    const QRectF area = layoutAreaPx(snap->style, canvas);
    for (const Divider &d : findDividers(m_cells)) {
        QVariantMap m;
        m.insert("vertical", d.vertical);
        if (d.vertical) {
            m.insert("pos", (area.x() + d.pos * area.width()) / canvas.width());
            m.insert("from", (area.y() + d.from * area.height()) / canvas.height());
            m.insert("to", (area.y() + d.to * area.height()) / canvas.height());
        } else {
            m.insert("pos", (area.y() + d.pos * area.height()) / canvas.height());
            m.insert("from", (area.x() + d.from * area.width()) / canvas.width());
            m.insert("to", (area.x() + d.to * area.width()) / canvas.width());
        }
        list.append(m);
    }
    return list;
}

int CollageStudio::cellAt(double x, double y) const
{
    // overflowing pictures lie over the others: look at them first, last drawn first
    for (int pass = 0; pass < 2; ++pass)
        for (int i = int(m_cells.size()) - 1; i >= 0; --i)
            if ((pass == 0) == m_cells[size_t(i)].content.overflow && canvasRectOf(m_cells[size_t(i)]).contains(x, y))
                return i;
    return -1;
}

// ---- pictures and layout ------------------------------------------------------------------------------------------------

void CollageStudio::applyRects(const std::vector<QRectF> &rects)
{
    for (size_t i = 0; i < m_cells.size() && i < rects.size(); ++i)
        m_cells[i].rect = rects[i];
}

void CollageStudio::setCellCount(int count)
{
    count = std::clamp(count, 1, kMaxCells);
    if (count == cellCount())
        return;
    const bool free = m_options.value("free").toBool();
    if (free) {
        while (cellCount() > count)
            m_cells.pop_back();
        while (cellCount() < count) {
            Cell c;
            c.rect = freeRect(cellCount());
            m_cells.push_back(c);
        }
    } else {
        // the pictures keep their order; the new layout is the same kind as the one in use, where there is one
        std::vector<Content> contents;
        for (const Cell &c : m_cells)
            contents.push_back(c.content);
        m_cells.assign(size_t(count), Cell{});
        const auto layouts = layoutsFor(count);
        m_presetIndex = std::clamp(m_presetIndex, 0, int(layouts.size()) - 1);
        for (size_t i = 0; i < m_cells.size(); ++i) {
            m_cells[i].rect = layouts[size_t(m_presetIndex)].rects[i];
            if (i < contents.size())
                m_cells[i].content = contents[i];
        }
    }
    if (m_selected >= count)
        m_selected = count - 1;
    emit selectedChanged();
    touch(true);
}

void CollageStudio::applyPreset(int index)
{
    const auto layouts = layoutsFor(cellCount());
    if (index < 0 || index >= int(layouts.size()))
        return;
    m_presetIndex = index;
    applyRects(layouts[size_t(index)].rects);
    touch(true);
}

// ---- saved templates ---------------------------------------------------------------------------------------------------

namespace {
// What a template remembers of how the collage looks: everything but how the file is written.
bool isLookOption(const QString &key) { return key != QLatin1String("format") && key != QLatin1String("quality"); }
}

QVariantList CollageStudio::templates() const
{
    QVariantList list;
    for (const Template &t : m_templates) {
        QVariantList rects;
        for (const Cell &c : t.cells)
            rects.append(QVariant(QVariantList{c.rect.x(), c.rect.y(), c.rect.width(), c.rect.height()}));
        list.append(QVariantMap{{"name", t.name}, {"count", int(t.cells.size())}, {"free", t.options.value("free").toBool()}, {"rects", rects}});
    }
    return list;
}

bool CollageStudio::saveTemplate(const QString &rawName)
{
    const QString name = rawName.trimmed();
    if (name.isEmpty())
        return false;
    Template t;
    t.name = name;
    for (auto it = m_options.constBegin(); it != m_options.constEnd(); ++it)
        if (isLookOption(it.key()))
            t.options.insert(it.key(), it.value());
    for (const Cell &c : m_cells) {
        Cell k;
        k.rect = c.rect;
        k.content.rotation = c.content.rotation;
        k.content.flipH = c.content.flipH;
        k.content.flipV = c.content.flipV;
        k.content.overflow = c.content.overflow;
        t.cells.push_back(k);
    }
    bool replaced = false;
    for (Template &old : m_templates)
        if (old.name.compare(name, Qt::CaseInsensitive) == 0) {
            old = t;
            replaced = true;
        }
    if (!replaced)
        m_templates.push_back(std::move(t));
    storeTemplates();
    emit templatesChanged();
    return true;
}

bool CollageStudio::applyTemplate(int index)
{
    if (index < 0 || index >= int(m_templates.size()))
        return false;
    const Template &t = m_templates[size_t(index)];
    for (auto it = t.options.constBegin(); it != t.options.constEnd(); ++it)
        if (m_options.contains(it.key()) && isLookOption(it.key())) {
            QVariant v = it.value();
            v.convert(m_options.value(it.key()).metaType()); // JSON has one kind of number
            m_options.insert(it.key(), v);
        }
    m_drag.active = false;
    std::vector<Content> contents;
    for (const Cell &c : m_cells)
        contents.push_back(c.content);
    m_cells = t.cells;
    for (size_t i = 0; i < m_cells.size(); ++i) {
        const Content turn = m_cells[i].content; // the template's turn, mirror and overflow
        m_cells[i].content = i < contents.size() ? contents[i] : Content{};
        m_cells[i].content.rotation = turn.rotation;
        m_cells[i].content.flipH = turn.flipH;
        m_cells[i].content.flipV = turn.flipV;
        m_cells[i].content.overflow = turn.overflow;
    }
    m_presetIndex = -1;
    if (m_selected >= cellCount())
        m_selected = cellCount() - 1;
    emit selectedChanged();
    emit optionsChanged();
    touch(true);
    return true;
}

void CollageStudio::deleteTemplate(int index)
{
    if (index < 0 || index >= int(m_templates.size()))
        return;
    m_templates.erase(m_templates.begin() + index);
    storeTemplates();
    emit templatesChanged();
}

void CollageStudio::setTemplatesFile(const QString &path)
{
    m_templatesFile = path;
    loadTemplates();
    emit templatesChanged();
}

void CollageStudio::loadTemplates()
{
    m_templates.clear();
    QFile file(m_templatesFile);
    if (m_templatesFile.isEmpty() || !file.open(QIODevice::ReadOnly))
        return;
    const QJsonArray list = QJsonDocument::fromJson(file.readAll()).object().value("templates").toArray();
    for (const QJsonValue &tv : list) {
        const QJsonObject o = tv.toObject();
        Template t;
        t.name = o.value("name").toString();
        const QJsonObject look = o.value("options").toObject();
        for (auto it = look.constBegin(); it != look.constEnd(); ++it)
            t.options.insert(it.key(), it.value().toVariant());
        for (const QJsonValue &cv : o.value("cells").toArray()) {
            const QJsonObject c = cv.toObject();
            Cell cell;
            cell.rect = QRectF(c.value("x").toDouble(), c.value("y").toDouble(), c.value("w").toDouble(), c.value("h").toDouble());
            cell.content.rotation = c.value("rotation").toDouble();
            cell.content.flipH = c.value("flipH").toBool();
            cell.content.flipV = c.value("flipV").toBool();
            cell.content.overflow = c.value("overflow").toBool();
            if (cell.rect.width() > 0.0 && cell.rect.height() > 0.0 && int(t.cells.size()) < kMaxCells)
                t.cells.push_back(cell);
        }
        if (!t.name.isEmpty() && !t.cells.empty())
            m_templates.push_back(std::move(t));
    }
}

void CollageStudio::storeTemplates() const
{
    if (m_templatesFile.isEmpty())
        return;
    QDir().mkpath(QFileInfo(m_templatesFile).absolutePath());
    QJsonArray list;
    for (const Template &t : m_templates) {
        QJsonObject o;
        o.insert("name", t.name);
        o.insert("options", QJsonObject::fromVariantMap(t.options));
        QJsonArray cells;
        for (const Cell &c : t.cells)
            cells.append(QJsonObject{{"x", c.rect.x()}, {"y", c.rect.y()}, {"w", c.rect.width()}, {"h", c.rect.height()},
                                     {"rotation", c.content.rotation}, {"flipH", c.content.flipH}, {"flipV", c.content.flipV},
                                     {"overflow", c.content.overflow}});
        o.insert("cells", cells);
        list.append(o);
    }
    QSaveFile out(m_templatesFile);
    if (out.open(QIODevice::WriteOnly)) {
        out.write(QJsonDocument(QJsonObject{{"version", 1}, {"templates", list}}).toJson(QJsonDocument::Indented));
        out.commit();
    }
}

void CollageStudio::newMosaic()
{
    ++m_seed;
    m_presetIndex = -1;
    applyRects(mosaicLayout(cellCount(), m_seed + 100).rects);
    touch(true);
}

int CollageStudio::addPaths(const QStringList &paths)
{
    int added = 0;
    for (const QString &raw : paths) {
        const QString path = pathFrom(raw);
        if (!QFileInfo(path).isFile())
            continue;
        int target = -1;
        for (int i = 0; i < cellCount(); ++i)
            if (m_cells[size_t(i)].content.path.isEmpty()) {
                target = i;
                break;
            }
        if (target < 0) {
            if (cellCount() >= kMaxCells)
                break;
            setCellCount(cellCount() + 1);
            target = cellCount() - 1;
        }
        m_cells[size_t(target)].content = Content{};
        m_cells[size_t(target)].content.path = path;
        ++added;
    }
    if (added > 0)
        touch(true);
    return added;
}

int CollageStudio::addUrls(const QVariantList &urls)
{
    QStringList paths;
    for (const QVariant &u : urls)
        paths << pathFrom(u.toString()); // from the page these arrive as text ("file:///C:/..."); pathFrom turns them into paths
    return addPaths(paths);
}

void CollageStudio::shufflePictures()
{
    std::vector<Content> contents;
    for (const Cell &c : m_cells)
        contents.push_back(c.content);
    std::mt19937 rng(std::random_device{}());
    std::shuffle(contents.begin(), contents.end(), rng);
    for (size_t i = 0; i < m_cells.size(); ++i)
        m_cells[i].content = contents[i];
    touch(true);
}

void CollageStudio::clearPictures()
{
    for (Cell &c : m_cells)
        c.content = Content{};
    touch(true);
}

void CollageStudio::select(int index)
{
    index = index >= 0 && index < cellCount() ? index : -1;
    if (index == m_selected)
        return;
    m_selected = index;
    emit selectedChanged();
}

// ---- one cell's picture -------------------------------------------------------------------------------------------------

void CollageStudio::setPicture(int cell, const QString &pathOrUrl)
{
    if (cell < 0 || cell >= cellCount())
        return;
    const QString path = pathFrom(pathOrUrl);
    if (!QFileInfo(path).isFile())
        return;
    m_cells[size_t(cell)].content = Content{};
    m_cells[size_t(cell)].content.path = path;
    touch(true);
}

void CollageStudio::clearPicture(int cell)
{
    if (cell < 0 || cell >= cellCount())
        return;
    m_cells[size_t(cell)].content = Content{};
    touch(true);
}

void CollageStudio::swapPictures(int a, int b)
{
    if (a < 0 || b < 0 || a >= cellCount() || b >= cellCount() || a == b)
        return;
    std::swap(m_cells[size_t(a)].content, m_cells[size_t(b)].content);
    touch(true);
}

void CollageStudio::panCell(int cell, double dx, double dy)
{
    if (cell < 0 || cell >= cellCount())
        return;
    const QRectF r = canvasRectOf(m_cells[size_t(cell)]);
    Content &c = m_cells[size_t(cell)].content;
    c.panX += dx / r.width();
    c.panY += dy / r.height();
    // never so far that the picture is out of sight
    c.panX = std::clamp(c.panX, -3.0, 3.0);
    c.panY = std::clamp(c.panY, -3.0, 3.0);
    // only the picture changes: the page does not read the pan, so the cells and lines are not rebuilt
    // (doing it on every mouse move is what made dragging a picture stutter)
    touch(false);
}

void CollageStudio::zoomCell(int cell, double factor, double atX, double atY)
{
    if (cell < 0 || cell >= cellCount() || factor <= 0.0)
        return;
    const QRectF r = canvasRectOf(m_cells[size_t(cell)]);
    Content &c = m_cells[size_t(cell)].content;
    const double z = c.zoom, nz = std::clamp(z * factor, 0.1, 10.0);
    if (nz == z)
        return;
    const double k = nz / z;
    // the point of the picture under the cursor stays under it: panning in cell units, about the cell's middle
    const double px = (atX - r.center().x()) / r.width(), py = (atY - r.center().y()) / r.height();
    c.panX = px - (px - c.panX) * k;
    c.panY = py - (py - c.panY) * k;
    c.zoom = nz;
    touch(false);
    emit layoutChanged();
}

void CollageStudio::setCellProperty(int cell, const QString &key, const QVariant &value)
{
    if (cell < 0 || cell >= cellCount())
        return;
    Content &c = m_cells[size_t(cell)].content;
    if (key == "zoom") c.zoom = std::clamp(value.toDouble(), 0.1, 10.0);
    else if (key == "rotation") c.rotation = std::clamp(value.toDouble(), -180.0, 180.0);
    else if (key == "flipH") c.flipH = value.toBool();
    else if (key == "flipV") c.flipV = value.toBool();
    else if (key == "overflow") c.overflow = value.toBool();
    else return;
    touch(false);
    emit layoutChanged();
}

void CollageStudio::resetPicture(int cell)
{
    if (cell < 0 || cell >= cellCount())
        return;
    Content &c = m_cells[size_t(cell)].content;
    const QString path = c.path;
    c = Content{};
    c.path = path;
    touch(false);
    emit layoutChanged();
}

// ---- dividing lines and free cells ------------------------------------------------------------------------------------

bool CollageStudio::beginDividerDrag(int dividerIndex)
{
    const auto found = findDividers(m_cells);
    if (m_options.value("free").toBool() || dividerIndex < 0 || dividerIndex >= int(found.size()))
        return false;
    m_drag.active = true;
    m_drag.divider = found[size_t(dividerIndex)];
    m_presetIndex = -1;
    return true;
}

void CollageStudio::dragDivider(double pos)
{
    if (!m_drag.active)
        return;
    const auto snap = snapshot();
    const QSize canvas = snap->style.size;
    const QRectF area = layoutAreaPx(snap->style, canvas);
    const double layoutPos = m_drag.divider.vertical ? (pos * canvas.width() - area.x()) / area.width()
                                                     : (pos * canvas.height() - area.y()) / area.height();
    moveDivider(m_cells, m_drag.divider, layoutPos);
    touch(true);
}

void CollageStudio::endDividerDrag()
{
    m_drag.active = false;
    emit layoutChanged();
}

void CollageStudio::moveCell(int cell, double dx, double dy)
{
    if (cell < 0 || cell >= cellCount())
        return;
    const QPointF d = toLayoutDelta(dx, dy);
    QRectF &r = m_cells[size_t(cell)].rect;
    r.moveLeft(std::clamp(r.left() + d.x(), 0.0, 1.0 - r.width()));
    r.moveTop(std::clamp(r.top() + d.y(), 0.0, 1.0 - r.height()));
    touch(true);
}

void CollageStudio::resizeCell(int cell, int handle, double dx, double dy)
{
    if (cell < 0 || cell >= cellCount())
        return;
    const QPointF d = toLayoutDelta(dx, dy);
    QRectF &r = m_cells[size_t(cell)].rect;
    double left = r.left(), top = r.top(), right = r.right(), bottom = r.bottom();
    const bool l = handle == 0 || handle == 6 || handle == 7, rt = handle == 2 || handle == 3 || handle == 4;
    const bool t = handle == 0 || handle == 1 || handle == 2, b = handle == 4 || handle == 5 || handle == 6;
    if (l) left = std::clamp(left + d.x(), 0.0, right - kMinCell);
    if (rt) right = std::clamp(right + d.x(), left + kMinCell, 1.0);
    if (t) top = std::clamp(top + d.y(), 0.0, bottom - kMinCell);
    if (b) bottom = std::clamp(bottom + d.y(), top + kMinCell, 1.0);
    r = QRectF(QPointF(left, top), QPointF(right, bottom));
    touch(true);
}

void CollageStudio::bringToFront(int cell)
{
    if (cell < 0 || cell >= cellCount() || cell == cellCount() - 1)
        return;
    Cell c = m_cells[size_t(cell)];
    m_cells.erase(m_cells.begin() + cell);
    m_cells.push_back(std::move(c));
    m_selected = cellCount() - 1;
    emit selectedChanged();
    touch(true);
}

// ---- options ------------------------------------------------------------------------------------------------------------

void CollageStudio::setOption(const QString &key, const QVariant &value)
{
    if (!m_options.contains(key))
        return;
    QVariant v = value;
    auto clampInt = [&](int lo, int hi) { v = std::clamp(value.toInt(), lo, hi); };
    auto clampReal = [&](double lo, double hi) { v = std::clamp(value.toDouble(), lo, hi); };
    if (key == "sizeW" || key == "sizeH") clampInt(100, 8000);
    else if (key == "background") clampInt(0, 2);
    else if (key == "gradientAngle") clampInt(-180, 180);
    else if (key == "photoBlur") clampReal(0, 30);
    else if (key == "margin") clampReal(0, 25);
    else if (key == "spacing") clampReal(0, 15);
    else if (key == "radius") clampReal(0, 100);
    else if (key == "borderWidth") clampReal(0, 8);
    else if (key == "shadow") clampReal(0, 100);
    else if (key == "shadowBlur") clampReal(0, 10);
    else if (key == "shadowOffset") clampReal(0, 6);
    else if (key == "format") clampInt(0, 2);
    else if (key == "quality") clampInt(1, 100);
    else if (key == "color1" || key == "color2" || key == "borderColor" || key == "cellFill") v = int(value.toUInt() & 0xFFFFFF);
    else if (m_options.value(key).typeId() == QMetaType::Bool) v = value.toBool();
    if (m_options.value(key) == v)
        return;
    m_options.insert(key, v);
    if (key == "free" && !v.toBool())
        m_drag.active = false;
    emit optionsChanged();
    touch(true);
}

// ---- drawing and saving ------------------------------------------------------------------------------------------------

QImage CollageStudio::preview(int longSide) const
{
    const auto snap = snapshot();
    const QSize full = snap->style.size;
    const double k = double(std::max(64, longSide)) / std::max(full.width(), full.height());
    const QSize out(std::max(8, int(std::lround(full.width() * k))), std::max(8, int(std::lround(full.height() * k))));
    PaneImageStore *store = m_store;
    return render(snap->cells, snap->style, [store](const QString &path, int maxSide) { return store->image(path, maxSide); }, out);
}

bool CollageStudio::exportTo(const QUrl &file)
{
    if (m_exporting)
        return false;
    const auto snap = snapshot();
    QString path = file.isLocalFile() ? file.toLocalFile() : file.toString();
    if (path.isEmpty())
        return false;
    const int format = std::clamp(m_options.value("format").toInt(), 0, 2);
    const int quality = m_options.value("quality").toInt();
    static const char *const suffixes[] = {"png", "jpg", "webp"};
    const QString wanted = QString::fromLatin1(suffixes[format]);
    const QString have = QFileInfo(path).suffix().toLower();
    if (have != wanted && !(format == 1 && have == QLatin1String("jpeg")))
        path += QLatin1Char('.') + wanted;

    m_exporting = true;
    m_cancel = std::make_shared<std::atomic<bool>>(false);
    emit exportingChanged();
    const auto cancel = m_cancel;
    PaneImageStore *store = m_store;
    QThreadPool::globalInstance()->start(QRunnable::create([this, snap, path, format, quality, cancel, store]() {
        bool ok = true;
        QString error;
        qint64 bytes = 0;
        QSize size = snap->style.size;
        if (format == 2 && (size.width() > 16383 || size.height() > 16383)) {
            ok = false;
            error = core::tr("El tamaño es demasiado grande para un WebP.");
        }
        QImage image;
        if (ok) {
            image = render(snap->cells, snap->style, [store](const QString &p, int maxSide) { return store->image(p, maxSide); }, size, cancel.get());
            if (cancel->load()) {
                ok = false;
                error = QStringLiteral("Cancelado.");
            } else if (image.isNull()) {
                ok = false;
                error = core::tr("No se pudo armar el collage.");
            }
        }
        if (ok) {
            QSaveFile out(path);
            if (!out.open(QIODevice::WriteOnly)) {
                ok = false;
                error = core::tr("No se pudo crear el archivo.");
            } else if (format == 2) {
                uint8_t *encoded = nullptr;
                const QImage rgba = image.convertToFormat(QImage::Format_RGBA8888);
                const size_t n = quality >= 100 ? WebPEncodeLosslessRGBA(rgba.constBits(), rgba.width(), rgba.height(), int(rgba.bytesPerLine()), &encoded)
                                                : WebPEncodeRGBA(rgba.constBits(), rgba.width(), rgba.height(), int(rgba.bytesPerLine()), float(quality), &encoded);
                ok = n > 0 && out.write(reinterpret_cast<const char *>(encoded), qint64(n)) == qint64(n);
                if (encoded)
                    WebPFree(encoded);
                if (!ok)
                    error = core::tr("No se pudo codificar el WebP.");
            } else {
                QImage toSave = image;
                if (format == 1) { // JPEG has no transparency: lay it over white
                    QImage flat(image.size(), QImage::Format_RGB32);
                    flat.fill(Qt::white);
                    QPainter p(&flat);
                    p.drawImage(0, 0, image);
                    p.end();
                    toSave = flat;
                }
                QImageWriter writer(&out, format == 1 ? "jpg" : "png");
                if (format == 1)
                    writer.setQuality(quality);
                ok = writer.write(toSave);
                if (!ok)
                    error = writer.errorString();
            }
            if (ok)
                ok = out.commit();
            else
                out.cancelWriting();
            if (ok)
                bytes = QFileInfo(path).size();
            else if (error.isEmpty())
                error = core::tr("No se pudo guardar el archivo.");
        }
        QMetaObject::invokeMethod(this, [this, ok, path, bytes, error]() {
            m_exporting = false;
            emit exportingChanged();
            emit exportFinished(ok, path, bytes, error);
        }, Qt::QueuedConnection);
    }));
    return true;
}

void CollageStudio::cancelExport()
{
    if (m_cancel)
        m_cancel->store(true);
}

QImage CollagePreviewProvider::requestImage(const QString &, QSize *size, const QSize &requestedSize)
{
    const int side = requestedSize.isValid() ? std::max(requestedSize.width(), requestedSize.height()) : 900;
    const QImage image = m_studio->preview(side);
    if (size)
        *size = image.size();
    return image;
}
