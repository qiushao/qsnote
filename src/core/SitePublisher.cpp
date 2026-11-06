#include "SitePublisher.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QLockFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>

namespace {
QString setting(const Notebook &book, const QString &key, const QString &fallback = {}) {
    const auto value = book.config[key].toString().trimmed();
    return value.isEmpty() ? fallback : value;
}

bool run(const QString &program, const QStringList &arguments, const QString &directory, QString *error,
         QByteArray *output = nullptr, const std::function<void(const QString &)> &progress = {}) {
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
    QByteArray standardError, pending;
    const auto readProgress = [&] {
        const auto data = process.readAllStandardError();
        standardError += data;
        if (!progress) return;
        pending += data;
        QString latest;
        qsizetype start = 0;
        for (qsizetype i = 0; i < pending.size(); ++i) {
            if (pending[i] != '\r' && pending[i] != '\n') continue;
            const auto line = QString::fromUtf8(pending.mid(start, i - start)).trimmed();
            if (!line.isEmpty()) latest = line;
            start = i + 1;
        }
        pending.remove(0, start);
        if (!latest.isEmpty()) progress(latest);
    };
    QObject::connect(&process, &QProcess::readyReadStandardError, &loop, readProgress);
    process.start(program, arguments);
    process.closeWriteChannel();
    timeout.start(300000);
    loop.exec();
    readProgress();
    if (progress && !pending.trimmed().isEmpty()) progress(QString::fromUtf8(pending).trimmed());
    const auto standardOutput = process.readAllStandardOutput();
    const auto result = standardOutput + standardError;
    if (output) *output = standardOutput;
    if (process.error() == QProcess::FailedToStart || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        *error = QCoreApplication::translate("SitePublisher", "发布命令失败：%1\n%2\n%3").arg(program,
            timeout.isActive() ? process.errorString() : QCoreApplication::translate("SitePublisher", "执行超时"), QString::fromUtf8(result).right(6000));
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
            *error = QCoreApplication::translate("SitePublisher", "请在站点配置中填写 GitHub 仓库地址。");
            return false;
        }
        const auto branch = setting(book, "githubBranch", "gh-pages");
        if (!QRegularExpression("^[A-Za-z0-9_][A-Za-z0-9_./-]*$").match(branch).hasMatch() ||
            branch.contains("..") || branch.contains("//") || branch.contains("/.") ||
            branch.endsWith('/') || branch.endsWith('.') || branch.endsWith(".lock")) {
            *error = QCoreApplication::translate("SitePublisher", "GitHub 发布分支名称无效。");
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
            *error = QCoreApplication::translate("SitePublisher", "请在站点配置中填写有效的 SSH 主机和用户名。");
            return false;
        }
        if (port < 1 || port > 65535) {
            *error = QCoreApplication::translate("SitePublisher", "SSH 端口必须为 1～65535。");
            return false;
        }
        // Both modern SFTP and legacy SCP accept this path without shell expansion.
        if (!QRegularExpression("^/[\\p{L}\\p{N}_./-]+$").match(path).hasMatch() || QDir::cleanPath(path) == "/") {
            *error = QCoreApplication::translate("SitePublisher", "远程目录须为绝对路径，可包含文字、数字、下划线、短横线和点，不能为根目录。");
            return false;
        }
        const auto key = setting(book, "sshIdentityFile");
        if (!key.isEmpty() && !QFileInfo(key).isFile()) {
            *error = QCoreApplication::translate("SitePublisher", "SSH 私钥文件不存在：") + key;
            return false;
        }
        programs = {"ssh", "scp"};
    } else {
        *error = QCoreApplication::translate("SitePublisher", "请先在站点配置中选择发布方式并填写对应设置。");
        return false;
    }
    for (const auto &program: programs) {
        if (QStandardPaths::findExecutable(program).isEmpty()) {
            *error = QCoreApplication::translate("SitePublisher", "发布站点需要 %1，请安装并加入 PATH。").arg(program);
            return false;
        }
    }
    return true;
}

bool SitePublisher::publish(const Notebook &book, QString *error, const ProgressCallback &progress) {
    if (!validate(book, error)) return false;
    const auto source = QDir(book.path).absoluteFilePath("public");
    if (!QFileInfo(source + "/index.html").isFile() || QFileInfo::exists(source + "/.git")) {
        *error = QCoreApplication::translate("SitePublisher", "站点生成结果无效，请重新生成站点，且不要在 public 中放置 .git。");
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
        if (progress) progress(0, 2, QCoreApplication::translate("SitePublisher", "正在连接 SSH 并创建远程目录：%1").arg(target + ":" + path));
        if (!run("ssh", options + QStringList{"-p", port, target, "mkdir -p -- '" + path + "'"}, book.path, error)) return false;
        if (progress) progress(1, 2, QCoreApplication::translate("SitePublisher", "正在通过 SCP 上传站点：%1\n等待传输完成，SCP 不提供字节进度。").arg(target + ":" + path));
        if (!run("scp", options + QStringList{"-B", "-r", "-P", port, source + "/.", target + ":" + path + "/"}, book.path, error)) return false;
        if (progress) progress(2, 2, QCoreApplication::translate("SitePublisher", "站点上传完成"));
        return true;
    }

    const auto repository = setting(book, "githubRepository");
    const auto branch = setting(book, "githubBranch", "gh-pages");
    const auto cacheKey = QCryptographicHash::hash((source + '\n' + repository + '\n' + branch).toUtf8(), QCryptographicHash::Sha256).toHex();
    const auto cache = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/site-publish/" + QString::fromLatin1(cacheKey);
    if (!QDir().mkpath(cache)) {
        *error = QCoreApplication::translate("SitePublisher", "无法创建发布缓存目录。");
        return false;
    }
    QLockFile lock(cache + "/publish.lock");
    lock.setStaleLockTime(0);
    if (!lock.tryLock()) {
        *error = QCoreApplication::translate("SitePublisher", "发布缓存正被使用，请稍后重试。");
        return false;
    }
    int completed = 0;
    QString operation;
    const auto report = [&](int step, const QString &message) {
        completed = step;
        operation = message;
        if (progress) progress(completed, 6, operation);
    };
    // Keep Git objects and the index in the cache, and stage public directly without copying it.
    const auto git = [&](const QStringList &arguments, QByteArray *output = nullptr) {
        return run("git", QStringList{"--git-dir=" + cache + "/repo.git", "--work-tree=" + source} + arguments,
                   source, error, output, [&](const QString &detail) {
            if (progress) progress(completed, 6, operation + '\n' + detail.right(500));
        });
    };
    report(0, QCoreApplication::translate("SitePublisher", "正在初始化发布仓库…"));
    if (!QFileInfo::exists(cache + "/repo.git/HEAD") &&
        !run("git", {"init", "--bare", "--quiet", cache + "/repo.git"}, source, error)) return false;
    if (!git({"config", "remote.origin.url", repository})) return false;
    QByteArray refs, cachedCommit;
    report(1, QCoreApplication::translate("SitePublisher", "正在检查远程分支：%1").arg(branch));
    if (!git({"ls-remote", "--heads", "origin", "refs/heads/" + branch}, &refs) ||
        !git({"for-each-ref", "--format=%(objectname)", "refs/heads/published"}, &cachedCommit)) return false;
    auto parent = refs.trimmed().split('\t').first();
    report(2, QCoreApplication::translate("SitePublisher", "正在准备发布分支：%1").arg(branch));
    if (!parent.isEmpty() && parent != cachedCommit.trimmed()) {
        if (!git({"fetch", "--no-tags", "--progress", "--depth=1", "origin", "refs/heads/" + branch}) ||
            !git({"rev-parse", "FETCH_HEAD"}, &parent)) return false;
        parent = parent.trimmed();
        if (!git({"update-ref", "refs/heads/published", QString::fromLatin1(parent)})) return false;
    }
    report(3, QCoreApplication::translate("SitePublisher", "正在暂存站点文件…"));
    if (!git({"add", "--all", "--force"})) return false;
    // Add an empty .nojekyll to the index without modifying the generated site.
    QByteArray emptyBlob, tree;
    if (!git({"hash-object", "-w", "--stdin"}, &emptyBlob) ||
        !git({"update-index", "--add", "--cacheinfo", "100644", QString::fromLatin1(emptyBlob.trimmed()), ".nojekyll"}) ||
        !git({"write-tree"}, &tree)) return false;
    if (!parent.isEmpty()) {
        QByteArray previousTree;
        if (!git({"rev-parse", QString::fromLatin1(parent) + "^{tree}"}, &previousTree)) return false;
        if (tree.trimmed() == previousTree.trimmed()) {
            report(6, QCoreApplication::translate("SitePublisher", "站点内容未变化，无需推送。"));
            return true;
        }
    }
    report(4, QCoreApplication::translate("SitePublisher", "正在创建发布提交…"));
    QStringList commitArguments{"-c", "user.name=QSNote", "-c", "user.email=qsnote@localhost", "-c", "commit.gpgsign=false",
                                "commit-tree", QString::fromLatin1(tree.trimmed()), "-m", "Publish site with QSNote"};
    if (!parent.isEmpty()) commitArguments << "-p" << QString::fromLatin1(parent);
    QByteArray commit;
    if (!git(commitArguments, &commit)) return false;
    // Retain the commit even if push fails; the next attempt still checks the actual remote tip.
    if (!git({"update-ref", "refs/heads/published", QString::fromLatin1(commit.trimmed())})) return false;
    report(5, QCoreApplication::translate("SitePublisher", "正在推送站点到远程分支：%1").arg(branch));
    if (!git({"push", "--progress", "origin", "refs/heads/published:refs/heads/" + branch})) return false;
    report(6, QCoreApplication::translate("SitePublisher", "推送完成，等待 GitHub Pages 部署"));
    return true;
}
