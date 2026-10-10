#include "NotebookStore.h"
#include "HexoSite.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QUuid>
#include <algorithm>
#include <functional>

namespace {
QString storageDir() { return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation); }

bool writeFile(const QString &path, const QByteArray &data, QString *error) {
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        *error = QStringLiteral("无法创建目录：") + QFileInfo(path).absolutePath();
        return false;
    }
    // Metadata-only site saves still check every post; keep unchanged file timestamps.
    QFile current(path);
    if (current.open(QIODevice::ReadOnly) && current.readAll() == data) return true;
    current.close();
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) == -1 || !file.commit()) {
        *error = file.errorString();
        return false;
    }
    return true;
}
}// namespace

QString NotebookStore::newId() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }

QString NotebookStore::typeLabel(const QString &type) {
    if (type == "site") return QStringLiteral("静态站点");
    if (type == "ebook") return QStringLiteral("电子书");
    return QStringLiteral("普通笔记本");
}

QString NotebookStore::storagePath() { return storageDir() + "/notebooks.json"; }

QString NotebookStore::indexFileName() { return QStringLiteral("_index.json"); }

QString NotebookStore::sanitized(const QString &name) {
    auto result = name;
    result.replace(QRegularExpression(QStringLiteral(R"([\\/:*?"<>|\x00-\x1f])")), QStringLiteral("_"));
    result = result.trimmed();
    while (result.endsWith('.')) result.chop(1);
    return result;
}

QString NotebookStore::defaultRoot() {
    auto documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (documents.isEmpty()) documents = QDir::homePath();
    return documents + "/qsnote";
}

QString NotebookStore::uniqueDirectory(const QString &root, const QString &name) {
    auto base = sanitized(name);
    if (base.isEmpty()) base = QStringLiteral("未命名");
    QString directory = root + "/" + base;
    if (!QFileInfo::exists(directory)) return directory;
    for (int index = 2;; ++index) {
        const QString candidate = directory + "-" + QString::number(index);
        if (!QFileInfo::exists(candidate)) return candidate;
    }
}

QString NotebookStore::chapterBase(const QString &path) {
    if (path.endsWith(QStringLiteral(".md"))) return path.left(path.size() - 3);
    return path;
}

QString NotebookStore::relativePath(const Notebook &book, const Note &note) {
    auto name = sanitized(note.title);
    if (name.isEmpty()) name = QStringLiteral("未命名");
    if (!note.folder && !name.endsWith(QStringLiteral(".md"))) name += QStringLiteral(".md");
    if (book.type == "site") {
        // Hexo 文章目录；分类继续用于组织源文件。
        const auto category = sanitized(note.metadata["category"].toString());
        return (note.metadata["draft"].toBool() ? "source/_drafts/" : "source/_posts/") + (category.isEmpty() ? name : category + "/" + name);
    }
    // 沿父链向上收集目录段：普通笔记本父级为文件夹；电子书父级章节的目录与章节文件同名（去掉 .md）。
    QStringList segments{name};
    QString parentId = note.parentId;
    QSet<QString> seen{note.id};
    while (!parentId.isEmpty()) {
        const Note *parent = nullptr;
        for (const auto &candidate: book.notes) {
            if (candidate.id == parentId && !seen.contains(candidate.id)) {
                parent = &candidate;
                break;
            }
        }
        if (!parent) break;
        seen.insert(parent->id);
        auto segment = sanitized(parent->title);
        if (segment.isEmpty()) segment = QStringLiteral("未命名");
        segments.prepend(book.type == "ebook" ? chapterBase(segment) : segment);
        parentId = parent->parentId;
    }
    return segments.join('/');
}

void NotebookStore::removeNoteFiles(const Notebook &book, const Note &note) {
    if (book.path.isEmpty() || note.path.isEmpty()) return;
    const QDir root(book.path);
    const QString absolute = root.absoluteFilePath(note.path);
    if (note.folder) {
        QDir(absolute).removeRecursively();
        return;
    }
    QFile::remove(absolute);
    if (book.type == "ebook") QDir(root.absoluteFilePath(chapterBase(note.path))).removeRecursively();
    // 清理变空的分类/章节目录
    auto parent = QFileInfo(absolute).absolutePath();
    while (parent.startsWith(book.path) && parent != book.path && QDir().rmdir(parent)) parent = QFileInfo(parent).absolutePath();
}

bool NotebookStore::syncNotebook(Notebook &book, QString *error) {
    if (book.path.isEmpty()) {
        *error = QStringLiteral("笔记本「%1」未设置保存目录").arg(book.name);
        return false;
    }
    if (!QDir().mkpath(book.path)) {
        *error = QStringLiteral("无法创建笔记本目录：") + book.path;
        return false;
    }
    if (book.type == "site" && !HexoSite::prepare(book, error)) return false;
    const QDir root(book.path);
    // 父先子后处理，移动父目录后子孙路径随前缀替换保持一致。
    QList<Note *> order;
    for (auto &note: book.notes) order.append(&note);
    const std::function<int(const Note &)> depth = [&](const Note &note) {
        int level = 0;
        const Note *current = &note;
        QSet<QString> seen{current->id};
        while (!current->parentId.isEmpty()) {
            const Note *parent = nullptr;
            for (const auto &candidate: book.notes) {
                if (candidate.id == current->parentId && !seen.contains(candidate.id)) {
                    parent = &candidate;
                    break;
                }
            }
            if (!parent) break;
            seen.insert(parent->id);
            ++level;
            current = parent;
        }
        return level;
    };
    std::stable_sort(order.begin(), order.end(), [&](const Note *first, const Note *second) { return depth(*first) < depth(*second); });
    QSet<QString> used;
    for (auto *note: order) {
        const QString oldPath = note->path;
        // 目标路径与同笔记本其它条目或磁盘残留冲突时追加序号。
        QString expected = relativePath(book, *note);
        const auto taken = [&](const QString &candidate) {
            return used.contains(candidate) || (candidate != oldPath && QFileInfo::exists(root.absoluteFilePath(candidate)));
        };
        if (taken(expected)) {
            const QString base = chapterBase(expected);
            const bool markdown = expected.endsWith(QStringLiteral(".md"));
            for (int index = 2;; ++index) {
                const QString candidate = base + "-" + QString::number(index) + (markdown ? QStringLiteral(".md") : QString());
                if (!taken(candidate)) {
                    expected = candidate;
                    break;
                }
            }
        }
        if (!oldPath.isEmpty() && oldPath != expected) {
            const QString oldAbsolute = root.absoluteFilePath(oldPath);
            const QString newAbsolute = root.absoluteFilePath(expected);
            if (QFileInfo::exists(oldAbsolute) && (!QDir().mkpath(QFileInfo(newAbsolute).absolutePath()) || !QDir().rename(oldAbsolute, newAbsolute))) {
                *error = QStringLiteral("无法将 %1 移动到 %2").arg(oldAbsolute, newAbsolute);
                return false;
            }
            // 电子书章节附带同名子章节目录；普通笔记本文件夹本身即目录。
            QString oldPrefix = oldPath + "/";
            QString newPrefix = expected + "/";
            if (book.type == "ebook") {
                const QString oldDirectory = root.absoluteFilePath(chapterBase(oldPath));
                const QString newDirectory = root.absoluteFilePath(chapterBase(expected));
                if (QFileInfo::exists(oldDirectory) && (!QDir().mkpath(QFileInfo(newDirectory).absolutePath()) || !QDir().rename(oldDirectory, newDirectory))) {
                    *error = QStringLiteral("无法将 %1 移动到 %2").arg(oldDirectory, newDirectory);
                    return false;
                }
                oldPrefix = chapterBase(oldPath) + "/";
                newPrefix = chapterBase(expected) + "/";
            }
            for (auto &child: book.notes) {
                if (child.path.startsWith(oldPrefix)) child.path = newPrefix + child.path.mid(oldPrefix.size());
            }
            // 清理移动后变空的目录（如旧分类目录）。
            auto parent = QFileInfo(oldAbsolute).absolutePath();
            while (parent.startsWith(book.path) && parent != book.path && QDir().rmdir(parent)) parent = QFileInfo(parent).absolutePath();
        }
        note->path = expected;
        used.insert(expected);
        if (note->folder) {
            if (!QDir().mkpath(root.absoluteFilePath(expected))) {
                *error = QStringLiteral("无法创建目录：") + root.absoluteFilePath(expected);
                return false;
            }
        } else if (book.type == "site" || note->contentDirty || !QFileInfo::exists(root.absoluteFilePath(expected))) {
            if (!writeFile(root.absoluteFilePath(expected), book.type == "site" ? HexoSite::postSource(*note) : note->content.toUtf8(), error)) return false;
            note->contentDirty = false;
        }
    }
    QJsonArray entries;
    for (const auto &note: book.notes) {
        entries.append(QJsonObject{{"id", note.id}, {"parentId", note.parentId}, {"title", note.title}, {"folder", note.folder}, {"path", note.path}, {"metadata", note.metadata}});
    }
    // 索引同时记录笔记本元信息，关闭笔记本后仍可凭目录完整恢复。
    const QJsonObject index{{"id", book.id}, {"name", book.name}, {"type", book.type}, {"config", book.config}, {"notes", entries}};
    return writeFile(root.absoluteFilePath(indexFileName()), QJsonDocument(index).toJson(), error);
}

bool NotebookStore::save(QString *error) {
    for (auto &book: notebooks) {
        if (!syncNotebook(book, error)) return false;
    }
    QJsonArray books;
    for (const auto &book: notebooks) {
        books.append(QJsonObject{{"id", book.id}, {"name", book.name}, {"type", book.type}, {"path", book.path}, {"config", book.config}});
    }
    return writeFile(storagePath(), QJsonDocument(books).toJson(), error);
}

bool NotebookStore::loadNotebook(Notebook &book, QString *error) {
    if (book.path.isEmpty()) return true;
    const QDir root(book.path);
    QFile index(root.absoluteFilePath(indexFileName()));
    if (!index.exists()) return true;  // 索引不存在视为空笔记本
    if (!index.open(QIODevice::ReadOnly)) {
        *error = index.errorString();
        return false;
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(index.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        *error = QStringLiteral("笔记本「%1」索引格式无效：").arg(book.name) + parseError.errorString();
        return false;
    }
    // 索引是笔记本的权威记录，优先于全局登记信息。
    const auto object = document.object();
    book.id = object["id"].toString(book.id);
    book.name = object["name"].toString(book.name);
    book.type = object["type"].toString(book.type);
    if (object.contains("config")) book.config = object["config"].toObject();
    QList<Note> loaded;
    for (const auto &entry: object["notes"].toArray()) {
        const auto item = entry.toObject();
        Note note;
        note.id = item["id"].toString();
        note.parentId = item["parentId"].toString();
        note.title = item["title"].toString();
        note.folder = item["folder"].toBool();
        note.path = item["path"].toString();
        note.metadata = item["metadata"].toObject();
        if (!note.folder) {
            QFile content(root.absoluteFilePath(note.path));
            if (content.open(QIODevice::ReadOnly)) {
                note.content = QString::fromUtf8(content.readAll());
                if (book.type == "site") note.content = HexoSite::postBody(note.content);
            }
        }
        loaded.append(note);
    }
    book.notes = loaded;
    return true;
}

bool NotebookStore::readNotebook(const QString &path, Notebook &book, QString *error) {
    if (!QFileInfo::exists(QDir(path).absoluteFilePath(indexFileName()))) {
        *error = QStringLiteral("所选目录不是 qsnote 笔记本（缺少 %1）").arg(indexFileName());
        return false;
    }
    book.path = path;
    if (!loadNotebook(book, error)) return false;
    if (book.id.isEmpty()) book.id = newId();
    if (book.type.isEmpty()) book.type = QStringLiteral("normal");
    if (book.name.isEmpty()) book.name = QDir(path).dirName();
    return true;
}

bool NotebookStore::load(QString *error) {
    notebooks.clear();
    QFile file(storagePath());
    if (!file.exists()) return true;
    if (!file.open(QIODevice::ReadOnly)) {
        *error = file.errorString();
        return false;
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isArray()) {
        *error = QStringLiteral("笔记数据格式无效：") + parseError.errorString();
        return false;
    }
    for (const auto &value: document.array()) {
        const auto object = value.toObject();
        Notebook book;
        book.id = object["id"].toString();
        book.name = object["name"].toString();
        book.type = object["type"].toString();
        book.path = object["path"].toString();
        book.config = object["config"].toObject();
        notebooks.append(book);
    }
    for (auto &book: notebooks) {
        if (!loadNotebook(book, error)) return false;
    }
    return true;
}

Notebook *NotebookStore::notebook(const QString &id) {
    for (auto &book: notebooks) {
        if (book.id == id) return &book;
    }
    return nullptr;
}

Note *NotebookStore::note(const QString &id) {
    for (auto &book: notebooks) {
        for (auto &note: book.notes) {
            if (note.id == id) return &note;
        }
    }
    return nullptr;
}

Notebook *NotebookStore::owner(const QString &noteId) {
    for (auto &book: notebooks) {
        for (const auto &note: book.notes) {
            if (note.id == noteId) return &book;
        }
    }
    return nullptr;
}
