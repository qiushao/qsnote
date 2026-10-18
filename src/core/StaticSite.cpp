#include "StaticSite.h"
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUrl>

namespace {
bool writeFile(const QString &path, const QByteArray &data, QString *error) {
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        *error = QCoreApplication::translate("StaticSite", "无法创建目录：") + QFileInfo(path).absolutePath();
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

}

bool StaticSite::copyDirectory(const QString &source, const QString &destination, QString *error) {
    if (!QDir().mkpath(destination)) {
        *error = QCoreApplication::translate("StaticSite", "无法创建目录：") + destination;
        return false;
    }
    QDirIterator files(source, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
    while (files.hasNext()) {
        const auto path = files.next();
        const auto target = destination + path.mid(source.size());
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

QString StaticSite::rootPath(const Notebook &book) {
    auto path = QUrl(book.config["domain"].toString()).path();
    if (!path.startsWith('/')) path.prepend('/');
    if (!path.endsWith('/')) path.append('/');
    return path;
}

bool StaticSite::prepare(Notebook &book, QString *error) {
    if (book.config["theme"].toString() == "stellar") book.config["theme"] = "next";
    if (book.config["theme"].toString() != "next") book.config["theme"] = "pure";
    if (!book.config.contains("commentPlatform")) book.config["commentPlatform"] = "none";
    for (const auto &directory: {"source/_posts", "source/_drafts", "public"}) {
        if (!QDir().mkpath(book.path + "/" + directory)) {
            *error = QCoreApplication::translate("StaticSite", "无法创建站点目录：") + directory;
            return false;
        }
    }
    const QMap<QString, QString> pages{{"categories", "分类"}, {"tags", "标签"}, {"links", "友链"}, {"about", "关于"}};
    for (auto it = pages.cbegin(); it != pages.cend(); ++it) {
        const auto path = book.path + "/source/" + it.key() + "/index.md";
        if (QFileInfo::exists(path)) continue;
        const auto source = "---\n" + QJsonDocument(QJsonObject{{"title", it.value()}, {"layout", it.key()}, {"comments", false}}).toJson() + "---\n";
        if (!writeFile(path, source, error)) return false;
    }
    return true;
}

QByteArray StaticSite::postSource(const Note &note) {
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
        metadata["permalink"] = slug.endsWith('/') || slug.endsWith(".html") ? slug : slug + "/";
    }
    return "---\n" + QJsonDocument(metadata).toJson() + "---\n" + note.content.toUtf8();
}

QString StaticSite::postBody(const QString &source) {
    if (!source.startsWith("---\n")) return source;
    const auto end = source.indexOf("\n---\n", 4);
    if (end < 0) return source;
    const auto header = QJsonDocument::fromJson(source.mid(4, end - 4).toUtf8()).object();
    return header["qsnote"].toBool() ? source.mid(end + 5) : source;
}

