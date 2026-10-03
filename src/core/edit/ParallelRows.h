#pragma once

#include <QRunnable>
#include <QSemaphore>
#include <QThread>
#include <QThreadPool>
#include <algorithm>

namespace core::edit {

// Dedicated pool for the row bands below, deliberately separate from
// QThreadPool::globalInstance(): AppController runs the whole save/export
// operation itself as a task on the GLOBAL pool, then blocks that task
// waiting for these row bands to finish. Splitting the bands across the
// global pool too would mean that blocked task and its own row bands are
// competing for the same limited slots - with enough concurrent bands
// requested, every slot can end up occupied by tasks blocked on each other,
// none of them ever running, and the wait below never returns.
inline QThreadPool &rowWorkerPool()
{
    static QThreadPool pool;
    static const bool init = [] {
        pool.setMaxThreadCount(std::max(1, QThread::idealThreadCount()));
        return true;
    }();
    Q_UNUSED(init);
    return pool;
}

// Splits [0, height) into per-core row bands and runs `rowFunc(y)` for every
// row in parallel, blocking until all of them finish. The per-pixel edit
// passes on an 8K (~33MP) image are a couple hundred milliseconds of genuine
// work - not something a smarter single-threaded loop can avoid, only
// spreading it across cores can. Callers must force `out` to detach (e.g.
// `out.bits()`) BEFORE calling this: QImage's copy-on-write detach isn't safe
// to trigger concurrently from multiple threads on the same image, only to
// read from afterward.
template <typename RowFunc>
void forEachRowParallel(int height, RowFunc &&rowFunc)
{
    if (height <= 0)
        return;
    const int threads = std::max(1, QThread::idealThreadCount());
    const int minRowsPerThread = 32; // not worth splitting a small image further
    if (threads <= 1 || height < threads * minRowsPerThread) {
        for (int y = 0; y < height; ++y)
            rowFunc(y);
        return;
    }

    const int band = (height + threads - 1) / threads;
    QSemaphore done;
    int bands = 0;
    for (int y0 = 0; y0 < height; y0 += band) {
        const int y1 = std::min(height, y0 + band);
        ++bands;
        rowWorkerPool().start(QRunnable::create([y0, y1, &rowFunc, &done]() {
            for (int y = y0; y < y1; ++y)
                rowFunc(y);
            done.release();
        }));
    }
    done.acquire(bands);
}

// Like forEachRowParallel, but hands out whole bands of `bandRows` rows at a
// time - `bandFunc(y0, y1)` covers [y0, y1). For passes that need scratch
// buffers or a few rows of context around the rows they produce, where doing
// the setup once per band beats doing it once per row.
template <typename BandFunc>
void forEachBandParallel(int height, int bandRows, BandFunc &&bandFunc)
{
    if (height <= 0)
        return;
    bandRows = std::max(1, bandRows);
    QSemaphore done;
    int bands = 0;
    for (int y0 = 0; y0 < height; y0 += bandRows) {
        const int y1 = std::min(height, y0 + bandRows);
        ++bands;
        rowWorkerPool().start(QRunnable::create([y0, y1, &bandFunc, &done]() {
            bandFunc(y0, y1);
            done.release();
        }));
    }
    done.acquire(bands);
}

} // namespace core::edit
