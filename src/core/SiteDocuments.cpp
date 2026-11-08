#include "SiteDocuments.h"
#include "SiteNavigationIcons.h"
#include "FrontMatter.h"
#include <QCoreApplication>
#include <QRegularExpression>
#include <QUrl>

namespace {
bool fail(QString *error, const QString &detail) {
    *error = QCoreApplication::translate("SiteDocuments", "站点文档配置无效：%1").arg(detail);
    return false;
}
}

bool SiteDocuments::isDocument(const Note &note) { return note.metadata["contentType"].toString() == "doc"; }

bool SiteDocuments::validSlug(const QString &slug) {
    static const QRegularExpression pattern(QStringLiteral("\\A[\\p{L}\\p{N}][\\p{L}\\p{N}_-]*\\z"));
    return pattern.match(slug).hasMatch();
}

const Note *SiteDocuments::find(const Notebook &book, const QString &id) {
    for (const auto &note: book.notes) if (note.id == id) return &note;
    return nullptr;
}

QJsonObject SiteDocuments::collection(const Notebook &book, const QString &id) {
    for (const auto &entry: book.config["docCollections"].toArray()) {
        const auto item = entry.toObject();
        if (item["id"].toString() == id) return item;
    }
    return {};
}

namespace {
QString documentPath(const Notebook &book, const Note &note, bool source) {
    const auto segment = [source](const Note &item) { return source ? NotebookStore::sanitized(item.title) : item.metadata["slug"].toString(); };
    auto name = segment(note);
    if (source && !note.folder && !name.endsWith(".md", Qt::CaseInsensitive)) name += ".md";
    QStringList parts{name};
    QSet<QString> seen{note.id};
    auto parentId = note.parentId;
    while (!parentId.isEmpty()) {
        const auto *parent = SiteDocuments::find(book, parentId);
        if (!parent || seen.contains(parentId)) return {};
        seen.insert(parentId);
        parts.prepend(segment(*parent));
        parentId = parent->parentId;
    }
    const auto group = SiteDocuments::collection(book, note.metadata["collectionId"].toString());
    return (source ? "source/docs/" : "docs/") + (source ? NotebookStore::sanitized(group["name"].toString()) : group["slug"].toString()) + '/' + parts.join('/') + (source ? QString() : QString("/"));
}
}

QString SiteDocuments::sourcePath(const Notebook &book, const Note &note) { return documentPath(book, note, true); }
QString SiteDocuments::route(const Notebook &book, const Note &note) { return documentPath(book, note, false); }

QMap<QString, QString> SiteDocuments::collectionMoves(const Notebook &book) {
    QMap<QString, QString> moves;
    for (const auto &note: book.notes) {
        if (!isDocument(note) || !note.path.startsWith("source/docs/")) continue;
        const auto before = note.path.section('/', 0, 2);
        const auto after = sourcePath(book, note).section('/', 0, 2);
        if (before != after) moves.insert(before, after);
    }
    return moves;
}

QList<const Note *> SiteDocuments::ordered(const Notebook &book, const QString &collectionId) {
    QList<const Note *> result, pending;
    for (auto it = book.notes.crbegin(); it != book.notes.crend(); ++it)
        if (isDocument(*it) && it->metadata["collectionId"].toString() == collectionId && it->parentId.isEmpty()) pending.append(&*it);
    QSet<QString> seen;
    while (!pending.isEmpty()) {
        const auto *note = pending.takeLast();
        if (seen.contains(note->id)) continue;
        seen.insert(note->id);
        result.append(note);
        for (auto it = book.notes.crbegin(); it != book.notes.crend(); ++it)
            if (it->parentId == note->id) pending.append(&*it);
    }
    return result;
}

bool SiteDocuments::validate(const Notebook &book, QString *error) {
    QSet<QString> ids, slugs, names, paths, sources;
    for (const auto &entry: book.config["docCollections"].toArray()) {
        const auto item = entry.toObject();
        const auto id = item["id"].toString(), slug = item["slug"].toString();
        const auto name = NotebookStore::sanitized(item["name"].toString()).toCaseFolded();
        if (id.isEmpty() || name.isEmpty() || names.contains(name) || !validSlug(slug) || ids.contains(id) || slugs.contains(slug.toCaseFolded()))
            return fail(error, QCoreApplication::translate("SiteDocuments", "集合名称、ID 或 Slug 无效或重复：%1").arg(slug));
        ids.insert(id);
        names.insert(name);
        slugs.insert(slug.toCaseFolded());
    }
    QSet<QString> noteIds;
    for (const auto &note: book.notes) {
        if (note.id.isEmpty() || noteIds.contains(note.id)) return fail(error, note.id);
        noteIds.insert(note.id);
        if (!isDocument(note)) continue;
        const auto collectionId = note.metadata["collectionId"].toString();
        if (!ids.contains(collectionId)) return fail(error, QCoreApplication::translate("SiteDocuments", "文档所属集合不存在：%1").arg(note.title));
        if (!validSlug(note.metadata["slug"].toString())) return fail(error, QCoreApplication::translate("SiteDocuments", "文档或目录 Slug 无效：%1").arg(note.metadata["slug"].toString()));
        QSet<QString> seen{note.id};
        auto parentId = note.parentId;
        while (!parentId.isEmpty()) {
            const auto *parent = find(book, parentId);
            if (!parent || !isDocument(*parent) || !parent->folder || parent->metadata["collectionId"].toString() != collectionId || seen.contains(parentId))
                return fail(error, QCoreApplication::translate("SiteDocuments", "目录层级无效：%1").arg(note.title));
            seen.insert(parentId);
            parentId = parent->parentId;
        }
        const auto source = sourcePath(book, note).toCaseFolded();
        if (NotebookStore::sanitized(note.title).isEmpty() || sources.contains(source))
            return fail(error, QCoreApplication::translate("SiteDocuments", "文档或目录名称无效或文件名冲突：%1").arg(note.title));
        sources.insert(source);
        const auto path = route(book, note).toCaseFolded();
        if (paths.contains(path)) return fail(error, QCoreApplication::translate("SiteDocuments", "文档或目录 Slug 冲突：%1").arg(path));
        paths.insert(path);
    }
    return true;
}

bool SiteDocuments::validateNavigation(const Notebook &book, QString *error) {
    QList<QPair<QJsonObject, int>> pending;
    for (const auto &value: book.config["navigation"].toArray()) pending.append({value.toObject(), 0});
    while (!pending.isEmpty()) {
        const auto entry = pending.takeLast();
        const auto &item = entry.first;
        const auto type = item["type"].toString(), target = item["target"].toString();
        QString iconError;
        if (!SiteNavigationIcons::validate(item, &iconError)) return fail(error, iconError);
        bool valid = !item["title"].toString().trimmed().isEmpty();
        if (type == "group") {
            valid = valid && entry.second == 0 && !item["children"].toArray().isEmpty() && target.isEmpty();
            for (const auto &child: item["children"].toArray()) pending.append({child.toObject(), entry.second + 1});
        } else if (type == "collection") valid = valid && !collection(book, target).isEmpty();
        else if (type == "note") {
            const auto *note = find(book, target);
            valid = valid && note && !note->folder && !note->metadata["draft"].toBool();
            if (valid && !isDocument(*note)) {
                auto body = note->content;
                QJsonObject metadata;
                valid = FrontMatter::parse(body, metadata, error) && !metadata["draft"].toBool() && metadata["published"] != QJsonValue(false);
            }
        } else if (type == "builtin") valid = valid && QStringList{"", "categories/", "tags/", "archives/", "links/", "about/"}.contains(target);
        else if (type == "external") {
            const QUrl url(target);
            valid = valid && url.isValid() && !url.host().isEmpty() && (url.scheme() == "https" || url.scheme() == "http");
        } else if (type == "path") {
            const QUrl url(target);
            valid = valid && target.startsWith('/') && !target.startsWith("//") && url.isRelative() && !target.contains('\\');
            for (const auto &part: url.path().split('/')) if (part == "." || part == ".." || part.contains(QChar::Null)) valid = false;
        } else valid = false;
        if (type != "group" && item.contains("children")) valid = false;
        if (!valid) return fail(error, QCoreApplication::translate("SiteDocuments", "导航目标无效，请修改或删除：%1").arg(item["title"].toString()));
    }
    return true;
}

bool SiteDocuments::move(Notebook &book, const QString &id, const QString &parentId, QString *error) {
    for (auto &note: book.notes) {
        if (note.id != id || !isDocument(note)) continue;
        const auto previous = note.parentId;
        note.parentId = parentId;
        if (validate(book, error)) return true;
        note.parentId = previous;
        return false;
    }
    return fail(error, id);
}

bool SiteDocuments::reorder(Notebook &book, const QString &id, int offset) {
    if (offset != -1 && offset != 1) return false;
    for (int i = 0; i < book.notes.size(); ++i) {
        const auto &note = book.notes[i];
        if (note.id != id || !isDocument(note)) continue;
        for (int j = i + offset; j >= 0 && j < book.notes.size(); j += offset) {
            const auto &other = book.notes[j];
            if (isDocument(other) && other.parentId == note.parentId && other.metadata["collectionId"] == note.metadata["collectionId"]) {
                book.notes.swapItemsAt(i, j);
                return true;
            }
        }
        break;
    }
    return false;
}
