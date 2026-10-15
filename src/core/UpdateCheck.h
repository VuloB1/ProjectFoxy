#pragma once

#include <QByteArray>
#include <QString>

namespace core {

// What the program needs to know about the newest published version, taken from GitHub's
// `GET /repos/<owner>/<repo>/releases/latest` answer (see app/UpdateManager).
struct ReleaseInfo {
    bool valid = false;
    QString version;       // "0.3.2" (the tag without its "v")
    QString pageUrl;       // the release's page, for "what is new"
    QString installerName; // ProjectFoxy-Setup-0.3.2.exe ("" when the release has none)
    QString installerUrl;
    QString sumsUrl;       // SHA256SUMS.txt
};

// True when `candidate` is a later version than `current`: "0.3.10" > "0.3.9", a leading "v" and a "-beta" tail
// are ignored, a missing part counts as 0. Anything that is not numbers compares as "not newer".
bool isNewerVersion(const QString &candidate, const QString &current);

// The answer of GitHub's "latest release" call. Not valid when the JSON is wrong, has no tag or no https page, and
// the download links are only taken when they are https links to GitHub.
ReleaseInfo parseLatestRelease(const QByteArray &json);

// The hash that a SHA256SUMS.txt (lines of "<64 hex>  <file name>", as `sha256sum` writes them) gives for
// `fileName`, lower case; empty when the file is not listed or the line is malformed.
QString expectedSha256(const QByteArray &sums, const QString &fileName);

// SHA-256 of a file, lower case hex; empty when it cannot be read.
QString sha256OfFile(const QString &path);

} // namespace core
