#include "HexoSite.h"
#include <QDir>
#include <QDirIterator>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrl>

namespace {
bool writeFile(const QString &path, const QByteArray &data, QString *error) {
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        *error = QStringLiteral("无法创建目录：") + QFileInfo(path).absolutePath();
        return false;
    }
    QFile current(path);
    if (current.open(QIODevice::ReadOnly) && current.readAll() == data) return true;
    current.close();
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        *error = path + ": " + file.errorString();
        return false;
    }
    return true;
}

bool run(const QString &program, const QStringList &arguments, const QString &directory, QString *error) {
    QProcess process;
    process.setWorkingDirectory(directory);
    process.setProcessChannelMode(QProcess::MergedChannels);
    QEventLoop loop;
    QObject::connect(&process, &QProcess::finished, &loop, &QEventLoop::quit);
    QObject::connect(&process, &QProcess::errorOccurred, &loop, &QEventLoop::quit);
    process.start(program, arguments);
    loop.exec();
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0 || process.error() == QProcess::FailedToStart) {
        *error = QStringLiteral("Hexo 命令失败：%1\n%2\n%3").arg(program, process.errorString(), QString::fromUtf8(process.readAll()).right(6000));
        return false;
    }
    return true;
}
}

bool HexoSite::copyDirectory(const QString &source, const QString &destination, QString *error) {
    if (!QDir().mkpath(destination)) {
        *error = QStringLiteral("无法创建目录：") + destination;
        return false;
    }
    QDirIterator files(source, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
    while (files.hasNext()) {
        const auto path = files.next();
        const auto target = destination + path.mid(source.size());
        // Resource extraction preserves local theme and scaffold edits.
        if (QFileInfo::exists(target)) continue;
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            *error = path + ": " + file.errorString();
            return false;
        }
        if (!writeFile(target, file.readAll(), error)) return false;
    }
    return true;
}

QString HexoSite::rootPath(const Notebook &book) {
    auto path = QUrl(book.config["domain"].toString()).path();
    if (!path.startsWith('/')) path.prepend('/');
    if (!path.endsWith('/')) path.append('/');
    return path;
}

bool HexoSite::prepare(Notebook &book, QString *error) {
    book.config["theme"] = "pure";
    if (!copyDirectory(":/hexo", book.path, error)) return false;
    for (const auto &directory: {"source/_posts", "source/_drafts", "source/_data", "public"}) {
        if (!QDir().mkpath(book.path + "/" + directory)) {
            *error = QStringLiteral("无法创建站点目录：") + directory;
            return false;
        }
    }
    auto domain = book.config["domain"].toString().trimmed();
    if (domain.isEmpty()) domain = "http://localhost";
    const auto &c = book.config;
    const QJsonObject theme{
        {"site", QJsonObject{{"title", c["title"].toString(book.name)}, {"footer", c["footer"].toString()}}},
        {"profile", QJsonObject{{"author", c["author"].toString()}, {"author_description", c["description"].toString()}, {"follow", domain}}},
        {"comment", QJsonObject{{"type", c["commentPlatform"].toString() == "none" ? QString() : c["commentPlatform"].toString()},
                                {"disqus", c["disqus"].toString()},
                                {"gitalk", QJsonObject{{"owner", c["commentOwner"].toString()}, {"admin", c["commentOwner"].toString()},
                                                       {"repo", c["commentRepo"].toString()}, {"ClientID", c["clientId"].toString()}, {"ClientSecret", c["clientSecret"].toString()}}}}}
    };
    // JSON is valid YAML; quote all user text without hand-written YAML escaping.
    const QJsonObject config{{"title", c["title"].toString(book.name)}, {"description", c["description"].toString()},
                             {"author", c["author"].toString()}, {"language", "zh-CN"}, {"url", domain}, {"root", rootPath(book)},
                             {"theme", "pure"}, {"source_dir", "source"}, {"public_dir", "public"},
                             {"permalink", ":title/"}, {"pretty_urls", QJsonObject{{"trailing_index", false}}},
                             {"highlight", QJsonObject{{"enable", true}}}, {"theme_config", theme}};
    if (!writeFile(book.path + "/_config.yml", QJsonDocument(config).toJson(), error)) return false;
    const QMap<QString, QString> pages{{"categories", "分类"}, {"tags", "标签"}, {"links", "友链"}, {"about", "关于"}};
    for (auto it = pages.cbegin(); it != pages.cend(); ++it) {
        const auto path = book.path + "/source/" + it.key() + "/index.md";
        if (QFileInfo::exists(path)) continue;
        const auto source = "---\n" + QJsonDocument(QJsonObject{{"title", it.value()}, {"layout", it.key()}, {"comments", false}}).toJson() + "---\n";
        if (!writeFile(path, source, error)) return false;
    }
    QJsonObject links;
    for (const auto &entry: c["links"].toArray()) {
        const auto link = entry.toObject();
        if (!link["name"].toString().isEmpty()) links[link["name"].toString()] = QJsonObject{{"link", link["url"]}, {"desc", ""}};
    }
    return writeFile(book.path + "/source/_data/links.json", QJsonDocument(links).toJson(), error);
}

QByteArray HexoSite::postSource(const Note &note) {
    // Explicit front matter edited in the Markdown body is owned by the author.
    if (note.content.startsWith("---\n") || note.content.startsWith("---\r\n")) return note.content.toUtf8();
    QJsonArray categories;
    if (!note.metadata["category"].toString().isEmpty()) categories.append(note.metadata["category"]);
    QJsonArray tags;
    for (const auto &tag: note.metadata["tags"].toString().split(QRegularExpression("[,，]"), Qt::SkipEmptyParts)) {
        if (!tag.trimmed().isEmpty()) tags.append(tag.trimmed());
    }
    QJsonObject metadata{{"title", note.title}, {"layout", "post"}, {"categories", categories}, {"tags", tags}, {"qsnote", true}};
    const auto date = note.metadata["date"].toString();
    if (!date.isEmpty()) metadata["date"] = date;
    const auto slug = note.metadata["slug"].toString().trimmed();
    if (!slug.isEmpty()) {
        metadata["slug"] = slug;
        metadata["permalink"] = slug.endsWith('/') ? slug : slug + "/";
    }
    return "---\n" + QJsonDocument(metadata).toJson() + "---\n" + note.content.toUtf8();
}

QString HexoSite::postBody(const QString &source) {
    if (!source.startsWith("---\n")) return source;
    const auto end = source.indexOf("\n---\n", 4);
    if (end < 0) return source;
    const auto header = QJsonDocument::fromJson(source.mid(4, end - 4).toUtf8()).object();
    return header["qsnote"].toBool() ? source.mid(end + 5) : source;
}

bool HexoSite::generate(const Notebook &book, QString *error) {
    const auto node = QStandardPaths::findExecutable("node");
    if (node.isEmpty()) {
        *error = QStringLiteral("生成 Hexo 站点需要 Node.js 18 或更新版本及 npm，请安装后重启 QSNote。");
        return false;
    }
    if (!QFileInfo::exists(book.path + "/node_modules/hexo/bin/hexo") || !QFileInfo::exists(book.path + "/node_modules/.package-lock.json")) {
#ifdef Q_OS_WIN
        const auto npm = QStandardPaths::findExecutable("npm.cmd");
#else
        const auto npm = QStandardPaths::findExecutable("npm");
#endif
        if (npm.isEmpty()) {
            *error = QStringLiteral("未找到 npm，无法安装 Hexo 站点依赖。");
            return false;
        }
#ifdef Q_OS_WIN
        if (!run(qEnvironmentVariable("COMSPEC", "cmd.exe"), {"/c", npm, "ci", "--no-audit", "--no-fund"}, book.path, error)) return false;
#else
        if (!run(npm, {"ci", "--no-audit", "--no-fund"}, book.path, error)) return false;
#endif
    }
    return run(node, {book.path + "/qsnote-generate.cjs"}, book.path, error);
}
