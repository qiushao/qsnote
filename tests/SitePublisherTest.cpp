#include "core/SitePublisher.h"
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

namespace {
bool write(const QString &path, const QByteArray &data) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
}
QByteArray read(const QString &path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}
QByteArray git(const QStringList &args) {
    QProcess process;
    process.start("git", args);
    if (!process.waitForFinished(30000) || process.exitCode() != 0) return {};
    return process.readAllStandardOutput();
}
}

// NOLINTBEGIN(readability-convert-member-functions-to-static)
class SitePublisherTest : public QObject {
    Q_OBJECT
private slots:
    void githubCreatesAndUpdatesBranch() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto remote = directory.path() + "/remote.git";
        git({"init", "--bare", remote});
        QVERIFY(QFileInfo::exists(remote + "/HEAD"));
        Notebook book;
        book.path = directory.path() + "/site";
        book.config = {{"publishMethod", "github"}, {"githubRepository", remote}};
        QVERIFY(write(book.path + "/public/index.html", "first"));
        QVERIFY(write(book.path + "/public/old.html", "old"));
        QVERIFY(write(book.path + "/public/.hidden", "hidden"));
        QVERIFY(write(book.path + "/_index.json", "private settings"));
        QString error;
        QVERIFY2(SitePublisher::publish(book, &error), qPrintable(error));
        QCOMPARE(git({"--git-dir", remote, "show", "gh-pages:index.html"}), QByteArray("first"));
        const auto files = git({"--git-dir", remote, "ls-tree", "--name-only", "gh-pages"});
        QVERIFY(files.contains(".nojekyll"));
        QVERIFY(files.contains(".hidden"));
        QVERIFY(!files.contains("_index.json"));
        QVERIFY(write(book.path + "/public/index.html", "second"));
        QVERIFY(QFile::remove(book.path + "/public/old.html"));
        QVERIFY2(SitePublisher::publish(book, &error), qPrintable(error));
        QCOMPARE(git({"--git-dir", remote, "show", "gh-pages:index.html"}), QByteArray("second"));
        QCOMPARE(git({"--git-dir", remote, "rev-list", "--count", "gh-pages"}).trimmed(), QByteArray("2"));
        QVERIFY(!git({"--git-dir", remote, "ls-tree", "--name-only", "gh-pages"}).contains("old.html"));
        QVERIFY2(SitePublisher::publish(book, &error), qPrintable(error)); // Unchanged output also succeeds.
        book.config["githubBranch"] = "preview/site";
        QVERIFY2(SitePublisher::publish(book, &error), qPrintable(error));
        QCOMPARE(git({"--git-dir", remote, "show", "preview/site:index.html"}), QByteArray("second"));
        book.config["githubRepository"] = directory.path() + "/missing.git";
        QVERIFY(!SitePublisher::publish(book, &error));
        QVERIFY(error.contains("git"));
    }

    void rejectsInvalidSettings() {
        Notebook book;
        QString error;
        QVERIFY(!SitePublisher::validate(book, &error));
        book.config = {{"publishMethod", "github"}};
        QVERIFY(!SitePublisher::validate(book, &error));
        book.config["githubRepository"] = "git@github.com:user/site.git";
        for (const auto &branch: {"-bad", "a..b", "a/.b", "a.lock", "a/", "a b"}) {
            book.config["githubBranch"] = branch;
            QVERIFY(!SitePublisher::validate(book, &error));
        }
        book.config = {{"publishMethod", "scp"}, {"sshHost", "example.com"}, {"sshUser", "deploy"}, {"sshDirectory", "/var/www/site"}};
        for (const auto &path: {"", "/", "/a/..", "relative", "/tmp/$(touch bad)", "/tmp/a;bad", "/tmp/a\nbad"}) {
            book.config["sshDirectory"] = path;
            QVERIFY(!SitePublisher::validate(book, &error));
        }
        book.config["sshDirectory"] = "/var/www/site";
        for (const auto &port: {"0", "65536", "abc", "-1"}) {
            book.config["sshPort"] = port;
            QVERIFY(!SitePublisher::validate(book, &error));
        }
        book.config["sshPort"] = "22";
        book.config["sshHost"] = "-oProxyCommand=bad";
        QVERIFY(!SitePublisher::validate(book, &error));
    }

    void scpArgumentsAndFailures() {
#ifdef Q_OS_WIN
        QSKIP("Executable shell fixtures require Unix");
#else
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto bin = directory.path() + "/bin";
        const auto log = directory.path() + "/commands";
        const auto oldPath = qgetenv("PATH");
        const auto restore = qScopeGuard([&] { qputenv("PATH", oldPath); });
        for (const auto &program: {"ssh", "scp"}) {
            QVERIFY(write(bin + "/" + program, QByteArray("#!/bin/sh\nprintf '%s\\n' \"$@\" >> '") + log.toUtf8() + "'\n"));
            QVERIFY(QFile::setPermissions(bin + "/" + program, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        }
        qputenv("PATH", bin.toUtf8() + ":" + oldPath);
        Notebook book;
        book.path = directory.path() + "/site with spaces";
        const auto key = directory.path() + "/key with spaces";
        QVERIFY(write(key, "test key"));
        QVERIFY(write(book.path + "/public/index.html", "site"));
        book.config = {{"publishMethod", "scp"}, {"sshHost", "example.com"}, {"sshUser", "deploy"},
                       {"sshPort", "2222"}, {"sshDirectory", "/var/www/site"}, {"sshIdentityFile", key}};
        QString error;
        QVERIFY2(SitePublisher::publish(book, &error), qPrintable(error));
        const auto commands = read(log);
        QVERIFY(commands.contains("-p\n2222\n"));
        QVERIFY(commands.contains("-P\n2222\n"));
        QVERIFY(commands.contains("-i\n" + key.toUtf8() + "\n"));
        QVERIFY(commands.contains("BatchMode=yes"));
        QVERIFY(commands.contains("StrictHostKeyChecking=yes"));
        QVERIFY(commands.contains("mkdir -p -- '/var/www/site'"));
        QVERIFY(commands.contains(book.path.toUtf8() + "/public/.\n"));
        QVERIFY(commands.contains("deploy@example.com:/var/www/site/"));
        QVERIFY(write(bin + "/scp", "#!/bin/sh\necho 'upload denied' >&2\nexit 1\n"));
        QVERIFY(!SitePublisher::publish(book, &error));
        QVERIFY(error.contains("upload denied"));
        QVERIFY(QFile::remove(log));
        QVERIFY(write(bin + "/ssh", "#!/bin/sh\necho 'connection denied' >&2\nexit 1\n"));
        QVERIFY(!SitePublisher::publish(book, &error));
        QVERIFY(error.contains("connection denied"));
        QVERIFY(!QFileInfo::exists(log));
#endif
    }
};
// NOLINTEND(readability-convert-member-functions-to-static)
QTEST_GUILESS_MAIN(SitePublisherTest)
#include "SitePublisherTest.moc"
