#include "UpdateCheck.h"

#include <QTemporaryFile>
#include <QtTest>

using namespace core;

namespace {
// JSON written with single quotes (easier to read inside a C++ string), turned into the real thing.
QByteArray js(QByteArray text)
{
    return text.replace('\'', '"');
}
} // namespace

class TestUpdateCheck : public QObject {
    Q_OBJECT

private slots:
    void versionsCompareNumberByNumber()
    {
        QVERIFY(isNewerVersion("0.3.2", "0.3.1"));
        QVERIFY(isNewerVersion("0.3.10", "0.3.9")); // not as text
        QVERIFY(isNewerVersion("1.0", "0.9.9"));
        QVERIFY(isNewerVersion("v0.4.0", "0.3.9"));
        QVERIFY(isNewerVersion("0.3.1.1", "0.3.1")); // a missing part is 0
        QVERIFY(!isNewerVersion("0.3.1", "0.3.1"));
        QVERIFY(!isNewerVersion("0.3.1", "0.3.1.0"));
        QVERIFY(!isNewerVersion("0.3.0", "0.3.1"));
        QVERIFY(!isNewerVersion("0.3.2-beta1", "0.3.2")); // a pre-release is not "newer" than its release
        QVERIFY(!isNewerVersion("hola", "0.3.1"));       // nonsense is never an update
        QVERIFY(!isNewerVersion("", "0.3.1"));
        QVERIFY(!isNewerVersion("0.3.2", ""));
    }

    void theLatestReleaseIsReadFromGithubsAnswer()
    {
        const QByteArray json = js(
            "{'tag_name': 'v0.3.2', 'html_url': 'https://github.com/VuloB1/ProjectFoxy/releases/tag/v0.3.2',"
            " 'assets': ["
            "  {'name': 'ProjectFoxy-0.3.2.flatpak', 'browser_download_url': 'https://github.com/x/y/releases/download/v0.3.2/ProjectFoxy-0.3.2.flatpak'},"
            "  {'name': 'ProjectFoxy-Setup-0.3.2.exe', 'browser_download_url': 'https://github.com/x/y/releases/download/v0.3.2/ProjectFoxy-Setup-0.3.2.exe'},"
            "  {'name': 'SHA256SUMS.txt', 'browser_download_url': 'https://github.com/x/y/releases/download/v0.3.2/SHA256SUMS.txt'}"
            "]}");
        const ReleaseInfo info = parseLatestRelease(json);
        QVERIFY(info.valid);
        QCOMPARE(info.version, QStringLiteral("0.3.2"));
        QCOMPARE(info.installerName, QStringLiteral("ProjectFoxy-Setup-0.3.2.exe"));
        QVERIFY(info.installerUrl.endsWith(QLatin1String("ProjectFoxy-Setup-0.3.2.exe")));
        QVERIFY(info.sumsUrl.endsWith(QLatin1String("SHA256SUMS.txt")));
    }

    void wrongOrSuspiciousAnswersAreNotTrusted()
    {
        QVERIFY(!parseLatestRelease("not json").valid);
        QVERIFY(!parseLatestRelease("{}").valid);
        QVERIFY(!parseLatestRelease(js("{'tag_name':'v1.0','html_url':'http://github.com/a/b'}")).valid); // not https
        QVERIFY(!parseLatestRelease(js("{'tag_name':'v1.0','html_url':'https://evil.example/a'}")).valid);
        QVERIFY(!parseLatestRelease(js("{'tag_name':'soon','html_url':'https://github.com/a/b'}")).valid);
        // a download that is not on GitHub is dropped, the release stays valid (it just cannot be installed from here)
        const ReleaseInfo info = parseLatestRelease(js(
            "{'tag_name':'v1.0','html_url':'https://github.com/a/b',"
            "'assets':[{'name':'ProjectFoxy-Setup-1.0.exe','browser_download_url':'https://evil.example/ProjectFoxy-Setup-1.0.exe'}]}"));
        QVERIFY(info.valid);
        QVERIFY(info.installerUrl.isEmpty());
    }

    void theChecksumIsFoundByFileName()
    {
        const QByteArray sums = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA  ProjectFoxy-portable-0.3.2.zip\r\n"
                                "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb *ProjectFoxy-Setup-0.3.2.exe\n"
                                "tooshort  ProjectFoxy-Other.exe\n";
        QCOMPARE(expectedSha256(sums, "ProjectFoxy-Setup-0.3.2.exe"), QString(64, QLatin1Char('b')));
        QCOMPARE(expectedSha256(sums, "ProjectFoxy-portable-0.3.2.zip"), QString(64, QLatin1Char('a'))); // lower-cased
        QVERIFY(expectedSha256(sums, "ProjectFoxy-Other.exe").isEmpty());
        QVERIFY(expectedSha256(sums, "missing.exe").isEmpty());
    }

    void aFilesHashIsTheOneOfItsBytes()
    {
        QTemporaryFile file;
        QVERIFY(file.open());
        file.write("abc");
        file.flush();
        // the well-known SHA-256 of "abc"
        QCOMPARE(sha256OfFile(file.fileName()),
                 QStringLiteral("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
        QVERIFY(sha256OfFile(file.fileName() + ".nope").isEmpty());
    }
};

QTEST_MAIN(TestUpdateCheck)
#include "test_update_check.moc"
