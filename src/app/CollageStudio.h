#pragma once

#include "collage/Collage.h"

#include <QImage>
#include <QMutex>
#include <QObject>
#include <QQuickImageProvider>
#include <QStringList>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <atomic>
#include <memory>
#include <vector>

class PaneImageStore;

// The collage maker ("Crear > Collage"): the cells and what is in them, how the collage looks, and writing it
// out as a picture. The page (qml/CollageStudio.qml) only shows this and forwards the mouse: dragging a
// picture inside its cell, dragging the dividing lines, moving and resizing cells in the free layout.
//
// Coordinates the page deals in are fractions (0..1) of the whole canvas, so they do not depend on the size
// the preview is drawn at or the size the collage is saved at.
class CollageStudio : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap options READ options NOTIFY optionsChanged)
    Q_PROPERTY(int cellCount READ cellCount NOTIFY layoutChanged)
    Q_PROPERTY(int selected READ selected NOTIFY selectedChanged)
    // [{name, rects: [[x, y, w, h]...]}] - the layouts ready-made for the current number of cells.
    Q_PROPERTY(QVariantList presets READ presets NOTIFY layoutChanged)
    Q_PROPERTY(int presetIndex READ presetIndex NOTIFY layoutChanged)
    // [{x, y, w, h, path, name, empty, zoom, panX, panY, rotation, flipH, flipV, overflow}] as drawn.
    Q_PROPERTY(QVariantList cells READ cells NOTIFY layoutChanged)
    // [{vertical, pos, from, to}] - the lines that can be dragged (empty in the free layout).
    Q_PROPERTY(QVariantList dividers READ dividers NOTIFY layoutChanged)
    Q_PROPERTY(int revision READ revision NOTIFY changed)
    Q_PROPERTY(bool exporting READ exporting NOTIFY exportingChanged)

public:
    explicit CollageStudio(PaneImageStore *store, QObject *parent = nullptr);
    ~CollageStudio() override;

    QVariantMap options() const { return m_options; }
    int cellCount() const { return int(m_cells.size()); }
    int selected() const { return m_selected; }
    QVariantList presets() const;
    int presetIndex() const { return m_presetIndex; }
    QVariantList cells() const;
    QVariantList dividers() const;
    int revision() const { return m_revision; }
    bool exporting() const { return m_exporting; }

    // ---- pictures and layout
    Q_INVOKABLE int addPaths(const QStringList &paths);
    Q_INVOKABLE int addUrls(const QVariantList &urls);
    Q_INVOKABLE void setCellCount(int count);
    Q_INVOKABLE void applyPreset(int index);
    Q_INVOKABLE void newMosaic();                       // another random layout for the same number of cells
    Q_INVOKABLE void shufflePictures();
    Q_INVOKABLE void clearPictures();
    Q_INVOKABLE void select(int index);
    // The cell under (x, y), fractions of the canvas; -1 where there is none.
    Q_INVOKABLE int cellAt(double x, double y) const;

    // ---- the picture of one cell
    Q_INVOKABLE void setPicture(int cell, const QString &pathOrUrl);
    Q_INVOKABLE void clearPicture(int cell);
    Q_INVOKABLE void swapPictures(int a, int b);
    Q_INVOKABLE void panCell(int cell, double dx, double dy);                  // fractions of the canvas
    Q_INVOKABLE void zoomCell(int cell, double factor, double atX, double atY); // the point stays under the cursor
    Q_INVOKABLE void setCellProperty(int cell, const QString &key, const QVariant &value); // zoom, rotation, flipH, flipV, overflow
    Q_INVOKABLE void resetPicture(int cell);

    // ---- dividing lines and free cells
    Q_INVOKABLE bool beginDividerDrag(int dividerIndex);
    Q_INVOKABLE void dragDivider(double pos);   // where it is now, a fraction of the canvas
    Q_INVOKABLE void endDividerDrag();
    Q_INVOKABLE void moveCell(int cell, double dx, double dy);                                 // free layout
    Q_INVOKABLE void resizeCell(int cell, int handle, double dx, double dy);                    // 0..7: TL T TR R BR B BL L
    Q_INVOKABLE void bringToFront(int cell);

    // ---- how it looks and how it is saved
    Q_INVOKABLE void setOption(const QString &key, const QVariant &value);
    Q_INVOKABLE bool exportTo(const QUrl &file);
    Q_INVOKABLE void cancelExport();

    // For the image provider (any thread): the collage at about `longSide` pixels along its long side.
    QImage preview(int longSide) const;

signals:
    void optionsChanged();
    void layoutChanged();
    void selectedChanged();
    void changed();
    void exportingChanged();
    void exportFinished(bool ok, const QString &path, qint64 bytes, const QString &error);

private:
    struct Snapshot {
        std::vector<core::collage::Cell> cells;
        core::collage::Style style;
    };
    core::collage::Style styleFromOptions() const;
    void touch(bool layout);
    void rebuildSnapshot();
    std::shared_ptr<const Snapshot> snapshot() const;
    QRectF canvasRectOf(const core::collage::Cell &cell) const;   // as drawn, fractions of the canvas
    QSize canvasSize() const;
    // maps a move along the canvas to one in the layout's own 0..1 units
    QPointF toLayoutDelta(double dx, double dy) const;
    void applyRects(const std::vector<QRectF> &rects);
    static QString pathFrom(const QString &pathOrUrl);

    PaneImageStore *m_store;
    std::vector<core::collage::Cell> m_cells;
    QVariantMap m_options;
    int m_selected = -1;
    int m_presetIndex = 0;
    unsigned m_seed = 1;
    int m_revision = 0;

    // a divider being dragged: the cells around it, as they were
    struct Drag {
        bool active = false;
        core::collage::Divider divider;
        std::vector<core::collage::Cell> cells;
    } m_drag;

    mutable QMutex m_mutex;
    std::shared_ptr<const Snapshot> m_snapshot;

    bool m_exporting = false;
    std::shared_ptr<std::atomic<bool>> m_cancel;
};

// "image://collageprev/<revision>": the collage as the page shows it.
class CollagePreviewProvider : public QQuickImageProvider {
public:
    explicit CollagePreviewProvider(CollageStudio *studio) : QQuickImageProvider(QQuickImageProvider::Image), m_studio(studio) {}
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

private:
    CollageStudio *m_studio;
};
