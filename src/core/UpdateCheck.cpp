#include "UpdateCheck.h"

#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QUrl>

namespace core {

namespace {

// "v0.3.2-beta1" -> {0, 3, 2}; empty when it does not start with a number.
QList<int> versionParts(QString text)
{
    text = text.trimmed();
    if (text.startsWith(QLatin1Char('v'), Qt::CaseInsensitive))
        text.remove(0, 1);
    text = text.section(QLatin1Char('-'), 0, 0).section(QLatin1Char('+'), 0, 0);
    QList<int> parts;
    for (const QString &piece : text.split(QLatin1Char('.'))) {
        bool ok = false;
        const int n = piece.toInt(&ok);
        if (!ok || n < 0)
            return {};
        parts << n;
    }
    return parts;
}

bool isGithubHttps(const QString &text)
{
    const QUrl url(text);
    return url.isValid() && url.scheme() == QLatin1String("https") && url.host() == QLatin1String("github.com");
}

} // namespace

bool isNewerVersion(const QString &candidate, const QString &current)
{
    const QList<int> a = versionParts(candidate), b = versionParts(current);
    if (a.isEmpty() || b.isEmpty())
        return false;
    for (qsizetype i = 0; i < qMax(a.size(), b.size()); ++i) {
        const int x = i < a.size() ? a.at(i) : 0, y = i < b.size() ? b.at(i) : 0;
        if (x != y)
            return x > y;
    }
    return false;
}

ReleaseInfo parseLatestRelease(const QByteArray &json)
{
    ReleaseInfo info;
    const QJsonObject root = QJsonDocument::fromJson(json).object();
    QString tag = root.value(QLatin1String("tag_name")).toString().trimmed();
    if (tag.startsWith(QLatin1Char('v'), Qt::CaseInsensitive))
        tag.remove(0, 1);
    const QString page = root.value(QLatin1String("html_url")).toString();
    if (versionParts(tag).isEmpty() || !isGithubHttps(page))
        return info;
    info.version = tag;
    info.pageUrl = page;

    static const QRegularExpression installerName(QStringLiteral("^ProjectFoxy-Setup-.+\\.exe$"));
    for (const QJsonValue &value : root.value(QLatin1String("assets")).toArray()) {
        const QJsonObject asset = value.toObject();
        const QString name = asset.value(QLatin1String("name")).toString();
        const QString url = asset.value(QLatin1String("browser_download_url")).toString();
        if (!isGithubHttps(url))
            continue;
        if (installerName.match(name).hasMatch()) {
            info.installerName = name;
            info.installerUrl = url;
        } else if (name == QLatin1String("SHA256SUMS.txt")) {
            info.sumsUrl = url;
        }
    }
    info.valid = true;
    return info;
}

QString expectedSha256(const QByteArray &sums, const QString &fileName)
{
    static const QRegularExpression line(QStringLiteral("^([0-9a-fA-F]{64})\\s+\\*?(.+?)\\s*$"));
    for (const QString &text : QString::fromUtf8(sums).split(QLatin1Char('\n'))) {
        const QRegularExpressionMatch m = line.match(text.trimmed());
        if (m.hasMatch() && m.captured(2) == fileName)
            return m.captured(1).toLower();
    }
    return {};
}

QString sha256OfFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file))
        return {};
    return QString::fromLatin1(hash.result().toHex());
}

} // namespace core
