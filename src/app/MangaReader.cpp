#include "MangaReader.h"
#include "AppPaths.h"
#include "PaneImageProvider.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QRunnable>
#include <QUrl>
#include <QUuid>

namespace {
constexpr int kRemembered = 200; // books whose page is kept
}

MangaReader::MangaReader(PaneImageStore *store, QObject *parent)
    : QObject(parent)
    , m_store(store)
    , m_store_settings(AppPaths::settingsFile(), QSettings::IniFormat)
{
    m_rightToLeft = m_store_settings.value(QStringLiteral("reader/rightToLeft"), true).toBool();
    m_doublePage = m_store_settings.value(QStringLiteral("reader/doublePage"), false).toBool();
    m_coverAlone = m_store_settings.value(QStringLiteral("reader/coverAlone"), true).toBool();
    const QString fit = m_store_settings.value(QStringLiteral("reader/fit"), QStringLiteral("screen")).toString();
    m_fit = (fit == QLatin1String("height") || fit == QLatin1String("width")) ? fit : QStringLiteral("screen");

    m_scanner.setMaxThreadCount(1);
    m_prefetcher.setMaxThreadCount(2);
    m_relayoutTimer.setSingleShot(true);
    m_relayoutTimer.setInterval(120);
    connect(&m_relayoutTimer, &QTimer::timeout, this, &MangaReader::relayout);

    // Pages unpacked by an earlier run that did not close its book (a crash) are of no use any more.
    QDir(AppPaths::cacheDir() + QStringLiteral("/comics")).removeRecursively();
}

MangaReader::~MangaReader()
{
    close();
}

bool MangaReader::isArchive(const QString &path)
{
    return core::ComicBook::isBookFile(path);
}

QString MangaReader::bookKey() const
{
    return QString::fromLatin1(QCryptographicHash::hash(m_path.toUtf8(), QCryptographicHash::Sha1).toHex());
}

bool MangaReader::open(const QString &location)
{
    close();
    // A path, or the "file:///..." a file dialog hands to QML.
    const QString path = location.startsWith(QLatin1String("file:")) ? QUrl(location).toLocalFile() : location;
    m_error.clear();
    const QFileInfo info(path);
    const bool isPicture = info.isFile() && !isArchive(path);
    const QString cache = AppPaths::cacheDir() + QStringLiteral("/comics/") + QUuid::createUuid().toString(QUuid::Id128);
    if (!m_book.open(path, cache)) {
        m_error = m_book.error();
        emit errorChanged();
        return false;
    }
    emit errorChanged();

    m_path = isPicture ? info.absolutePath() : info.absoluteFilePath();
    m_sizes = QVector<QSize>(m_book.pageCount());
    m_scanned = false;
    // From a picture: that page. From the book itself: where it was left.
    m_page = isPicture ? m_book.pageOfFile(path)
                       : qBound(0, m_store_settings.value(QStringLiteral("reader/pos/") + bookKey(), 0).toInt(),
                                m_book.pageCount() - 1);
    m_active = true;
    relayout();
    showPage(m_page);
    if (m_doublePage)
        scanShapes();
    emit activeChanged();
    return true;
}

void MangaReader::close()
{
    if (!m_active && m_book.pageCount() == 0)
        return;
    ++m_generation;
    m_scanner.waitForDone();
    m_prefetcher.waitForDone();
    m_relayoutTimer.stop();
    m_book.close();
    m_sizes.clear();
    m_spreads.clear();
    m_spread = m_page = 0;
    m_active = false;
    emit activeChanged();
    emit positionChanged();
}

int MangaReader::currentPage() const
{
    return m_spread >= 0 && m_spread < m_spreads.size() ? m_spreads[m_spread].first : m_page;
}

int MangaReader::lastPage() const
{
    return m_spread >= 0 && m_spread < m_spreads.size() ? m_spreads[m_spread].first + m_spreads[m_spread].count - 1 : 0;
}

QVariantList MangaReader::pages()
{
    QVariantList out;
    if (!m_active || m_spread < 0 || m_spread >= m_spreads.size())
        return out;
    const core::Spread s = m_spreads[m_spread];
    for (int i = s.first; i < s.first + s.count; ++i) {
        const QSize size = m_sizes.value(i);
        out.append(QVariantMap{{"path", m_book.pageFile(i)}, {"width", size.width()}, {"height", size.height()}, {"page", i}});
    }
    // Reading right to left, the first page of a double spread is the one on the right.
    if (m_rightToLeft && out.size() == 2)
        out.swapItemsAt(0, 1);
    return out;
}

void MangaReader::relayout()
{
    if (!m_active)
        return;
    const QVector<core::Spread> spreads = core::buildSpreads(m_sizes, m_doublePage, m_coverAlone);
    if (spreads == m_spreads)
        return;
    m_spreads = spreads;
    // `m_page` is the page the reader asked for, not the first of its spread: when the shapes arrive and the pairs
    // move, the spread that holds that page is the one to show.
    m_spread = core::spreadOfPage(m_spreads, m_page);
    emit positionChanged();
    prefetch();
}

void MangaReader::showPage(int page)
{
    if (!m_active || m_spreads.isEmpty())
        return;
    m_page = qBound(0, page, m_book.pageCount() - 1);
    m_spread = core::spreadOfPage(m_spreads, m_page);
    emit positionChanged();
    prefetch();
    remember();
}

void MangaReader::next()
{
    if (m_active && m_spread + 1 < m_spreads.size())
        showPage(m_spreads[m_spread + 1].first);
}

void MangaReader::previous()
{
    if (m_active && m_spread > 0)
        showPage(m_spreads[m_spread - 1].first);
}

void MangaReader::first() { showPage(0); }

void MangaReader::last() { showPage(m_book.pageCount() - 1); }

void MangaReader::goToPage(int page) { showPage(page); }

void MangaReader::remember()
{
    const QString key = QStringLiteral("reader/pos/") + bookKey();
    m_store_settings.setValue(key, currentPage());
    QStringList recent = m_store_settings.value(QStringLiteral("reader/recent")).toStringList();
    recent.removeAll(bookKey());
    recent.prepend(bookKey());
    while (recent.size() > kRemembered)
        m_store_settings.remove(QStringLiteral("reader/pos/") + recent.takeLast());
    m_store_settings.setValue(QStringLiteral("reader/recent"), recent);
}

// The shape of every page, in the background and starting at the page being read: double pages need to know which
// ones are wide. Pages in the order of the book from the one on screen, then the ones before it.
void MangaReader::scanShapes()
{
    if (m_scanned || !m_active)
        return;
    m_scanned = true;
    const int generation = m_generation.load();
    const int start = currentPage();
    m_scanner.start(QRunnable::create([this, generation, start]() {
        const int n = m_book.pageCount();
        for (int k = 0; k < n; ++k) {
            const int i = (start + k) % n;
            if (generation != m_generation.load())
                return;
            const QSize size = m_book.pageSize(i);
            QMetaObject::invokeMethod(
                this,
                [this, generation, i, size]() {
                    if (generation != m_generation.load() || i >= m_sizes.size() || m_sizes[i] == size)
                        return;
                    m_sizes[i] = size;
                    if (!m_relayoutTimer.isActive())
                        m_relayoutTimer.start();
                },
                Qt::QueuedConnection);
        }
    }));
}

// The next spreads (and the one before) are unpacked and decoded ahead, with the same size the view will ask for, so
// turning the page does not wait.
void MangaReader::prefetch()
{
    if (!m_active)
        return;
    QVector<int> pages;
    for (int s : {m_spread + 1, m_spread + 2, m_spread - 1})
        if (s >= 0 && s < m_spreads.size())
            for (int i = 0; i < m_spreads[s].count; ++i)
                pages.append(m_spreads[s].first + i);
    const int generation = m_generation.load();
    const int side = m_pageSide;
    for (int page : std::as_const(pages))
        m_prefetcher.start(QRunnable::create([this, generation, page, side]() {
            if (generation != m_generation.load())
                return;
            const QString file = m_book.pageFile(page);
            if (!file.isEmpty())
                m_store->image(file, side);
        }));
}

void MangaReader::setRightToLeft(bool on)
{
    if (m_rightToLeft == on)
        return;
    m_rightToLeft = on;
    m_store_settings.setValue(QStringLiteral("reader/rightToLeft"), on);
    emit settingsChanged();
    emit positionChanged(); // the order of a double page
}

void MangaReader::setDoublePage(bool on)
{
    if (m_doublePage == on)
        return;
    m_doublePage = on;
    m_store_settings.setValue(QStringLiteral("reader/doublePage"), on);
    if (on)
        scanShapes();
    relayout();
    emit settingsChanged();
}

void MangaReader::setCoverAlone(bool on)
{
    if (m_coverAlone == on)
        return;
    m_coverAlone = on;
    m_store_settings.setValue(QStringLiteral("reader/coverAlone"), on);
    relayout();
    emit settingsChanged();
}

void MangaReader::setFit(const QString &fit)
{
    const QString value = (fit == QLatin1String("height") || fit == QLatin1String("width")) ? fit : QStringLiteral("screen");
    if (m_fit == value)
        return;
    m_fit = value;
    m_store_settings.setValue(QStringLiteral("reader/fit"), value);
    emit settingsChanged();
}

void MangaReader::setPageSide(int side)
{
    side = qBound(512, side, 8192);
    if (m_pageSide == side)
        return;
    m_pageSide = side;
    emit pageSideChanged();
    prefetch();
}
