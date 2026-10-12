#include "SitePublisher.h"
#include "HexoSite.h"
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>

namespace {
QString setting(const Notebook &book, const QString &key, const QString &fallback = {}) {
    const auto value = book.config[key].toString().trimmed();
    return value.isEmpty() ? fallback : value;
}

bool run(const QString &program, const QStringList &arguments, const QString &directory, QString *error, QByteArray *output = nullptr) {
    QProcess process;
    process.setWorkingDirectory(directory);
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert("GIT_TERMINAL_PROMPT", "0");
    environment.insert("GCM_INTERACTIVE", "Never");
    if (!environment.contains("GIT_SSH_COMMAND"))
        environment.insert("GIT_SSH_COMMAND", "ssh -oBatchMode=yes -oStrictHostKeyChecking=yes -oConnectTimeout=15");
    process.setProcessEnvironment(environment);
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &process, &QProcess::kill);
    QObject::connect(&process, &QProcess::finished, &loop, &QEventLoop::quit);
    QObject::connect(&process, &QProcess::errorOccurred, &loop, &QEventLoop::quit);
    process.start(program, arguments);
    process.closeWriteChannel();
    timeout.start(300000);
    loop.exec();
    const auto standardOutput = process.readAllStandardOutput();
    const auto result = standardOutput + process.readAllStandardError();
    if (output) *output = standardOutput;
    if (process.error() == QProcess::FailedToStart || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        *error = QStringLiteral("发布命令失败：%1\n%2\n%3").arg(program,
            timeout.isActive() ? process.errorString() : QStringLiteral("执行超时"), QString::fromUtf8(result).right(6000));
        return false;
    }
    return true;
}
}

bool SitePublisher::validate(const Notebook &book, QString *error) {
    const auto method = setting(book, "publishMethod");
    QStringList programs;
    if (method == "github") {
        const auto repository = setting(book, "githubRepository");
        if (repository.isEmpty() || repository.startsWith('-') || repository.contains('\n')) {
            *error = QStringLiteral("请在站点配置中填写 GitHub 仓库地址。");
            return false;
        }
        const auto branch = setting(book, "githubBranch", "gh-pages");
        if (!QRegularExpression("^[A-Za-z0-9_][A-Za-z0-9_./-]*$").match(branch).hasMatch() ||
            branch.contains("..") || branch.contains("//") || branch.contains("/.") ||
            branch.endsWith('/') || branch.endsWith('.') || branch.endsWith(".lock")) {
            *error = QStringLiteral("GitHub 发布分支名称无效。");
            return false;
        }
        programs = {"git"};
    } else if (method == "scp") {
        const auto host = setting(book, "sshHost");
        const auto user = setting(book, "sshUser");
        const auto path = setting(book, "sshDirectory");
        const int port = setting(book, "sshPort", "22").toInt();
        if (!QRegularExpression("^[A-Za-z0-9][A-Za-z0-9.:-]*$").match(host).hasMatch() ||
            !QRegularExpression("^[A-Za-z0-9_][A-Za-z0-9_.-]*$").match(user).hasMatch()) {
            *error = QStringLiteral("请在站点配置中填写有效的 SSH 主机和用户名。");
            return false;
        }
        if (port < 1 || port > 65535) {
            *error = QStringLiteral("SSH 端口必须为 1～65535。");
            return false;
        }
        // Both modern SFTP and legacy SCP accept this path without shell expansion.
        if (!QRegularExpression("^/[\\p{L}\\p{N}_./-]+$").match(path).hasMatch() || QDir::cleanPath(path) == "/") {
            *error = QStringLiteral("远程目录须为绝对路径，可包含文字、数字、下划线、短横线和点，不能为根目录。");
            return false;
        }
        const auto key = setting(book, "sshIdentityFile");
        if (!key.isEmpty() && !QFileInfo(key).isFile()) {
            *error = QStringLiteral("SSH 私钥文件不存在：") + key;
            return false;
        }
        programs = {"ssh", "scp"};
    } else {
        *error = QStringLiteral("请先在站点配置中选择发布方式并填写对应设置。");
        return false;
    }
    for (const auto &program: programs) {
        if (QStandardPaths::findExecutable(program).isEmpty()) {
            *error = QStringLiteral("发布站点需要 %1，请安装并加入 PATH。").arg(program);
            return false;
        }
    }
    return true;
}

bool SitePublisher::publish(const Notebook &book, QString *error) {
    if (!validate(book, error)) return false;
    const auto source = QDir(book.path).absoluteFilePath("public");
    if (!QFileInfo(source + "/index.html").isFile() || QFileInfo::exists(source + "/.git")) {
        *error = QStringLiteral("站点生成结果无效，请重新生成站点，且不要在 public 中放置 .git。");
        return false;
    }
    if (setting(book, "publishMethod") == "scp") {
        const auto host = setting(book, "sshHost");
        const auto target = setting(book, "sshUser") + "@" + (host.contains(':') ? "[" + host + "]" : host);
        const auto port = setting(book, "sshPort", "22");
        const auto path = QDir::cleanPath(setting(book, "sshDirectory"));
        QStringList options{"-o", "BatchMode=yes", "-o", "StrictHostKeyChecking=yes", "-o", "ConnectTimeout=15"};
        const auto key = setting(book, "sshIdentityFile");
        if (!key.isEmpty()) options << "-i" << key;
        if (!run("ssh", options + QStringList{"-p", port, target, "mkdir -p -- '" + path + "'"}, book.path, error)) return false;
        return run("scp", options + QStringList{"-B", "-r", "-P", port, source + "/.", target + ":" + path + "/"}, book.path, error);
    }

    QTemporaryDir checkout;
    if (!checkout.isValid()) {
        *error = QStringLiteral("无法创建发布临时目录。");
        return false;
    }
    const auto git = [&](const QStringList &arguments, QByteArray *output = nullptr) {
        return run("git", arguments, checkout.path(), error, output);
    };
    const auto branch = setting(book, "githubBranch", "gh-pages");
    if (!git({"init", "--quiet"}) || !git({"remote", "add", "origin", setting(book, "githubRepository")})) return false;
    QByteArray refs;
    if (!git({"ls-remote", "--heads", "origin", "refs/heads/" + branch}, &refs)) return false;
    if (!refs.trimmed().isEmpty()) {
        if (!git({"fetch", "--depth=1", "origin", "refs/heads/" + branch}) || !git({"checkout", "-B", branch, "FETCH_HEAD"})) return false;
        // Only the disposable checkout is cleared; preserve branch history and use a normal push.
        for (const auto &entry: QDir(checkout.path()).entryInfoList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot)) {
            if (entry.fileName() == ".git") continue;
            const bool removed = entry.isDir() && !entry.isSymLink() ? QDir(entry.filePath()).removeRecursively() : QFile::remove(entry.filePath());
            if (!removed) {
                *error = QStringLiteral("无法清理发布临时文件：") + entry.filePath();
                return false;
            }
        }
    }
    if (!HexoSite::copyDirectory(source, checkout.path(), error)) return false;
    QFile nojekyll(checkout.path() + "/.nojekyll");
    if (!nojekyll.open(QIODevice::WriteOnly)) {
        *error = nojekyll.errorString();
        return false;
    }
    nojekyll.close();
    return git({"add", "--all", "--force"}) &&
           git({"-c", "user.name=QSNote", "-c", "user.email=qsnote@localhost", "-c", "commit.gpgsign=false",
                "commit", "--allow-empty", "-m", "Publish site with QSNote"}) &&
           git({"push", "origin", "HEAD:refs/heads/" + branch});
}
