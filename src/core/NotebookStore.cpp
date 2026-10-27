#include "NotebookStore.h"
#include "ArticleRenderer.h"
#include "AtomicFile.h"
#include "StaticSite.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMimeDatabase>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUrl>
#include <QUuid>
#include <algorithm>
#include <functional>
#include <vtextedit/markdownutils.h>

namespace {
QString storageDir() { return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation); }

bool copyInsertedImages(QString &content, const QString &oldDirectory, const QString &newDirectory, QString *error) {
    if (oldDirectory == newDirectory) return true;
    const auto images = vte::MarkdownUtils::fetchImageLinks(content, oldDirectory, vte::MarkdownLink::LocalRelativeInternal);
    for (const auto &image: images) {
        const QFileInfo sourceInfo(image.m_path);
        // Images inserted by the editor live directly in resources/images. Use
        // parser spans so code examples and unrelated links remain untouched.
        if (!image.hasUrlSpan() || !image.m_exists
            || sourceInfo.absolutePath() != QDir(oldDirectory).filePath("resources/images")) continue;
        QFile source(image.m_path);
        if (!source.open(QIODevice::ReadOnly)) {
            *error = source.fileName() + ": " + source.errorString();
            return false;
        }
        const auto bytes = source.readAll();
        if (source.error() != QFileDevice::NoError) {
            *error = source.fileName() + ": " + source.errorString();
            return false;
        }
        const QDir destination(QDir(newDirectory).filePath("resources/images"));
        auto name = sourceInfo.fileName();
        for (int index = 1; QFileInfo::exists(destination.filePath(name)); ++index) {
            QFile existing(destination.filePath(name));
            if (existing.open(QIODevice::ReadOnly) && existing.readAll() == bytes) break;
            name = sourceInfo.completeBaseName() + '-' + QString::number(index) + '.' + sourceInfo.suffix();
        }
        // Keep the source: other notes in the old directory may still use it.
        if (!QFileInfo::exists(destination.filePath(name)) && !AtomicFile::write(destination.filePath(name), bytes, error)) return false;
        if (name != sourceInfo.fileName()) {
            const auto url = "resources/images/" + QString::fromLatin1(QUrl::toPercentEncoding(name));
            content.replace(image.m_urlStart, image.m_urlEnd - image.m_urlStart, url);
        }
    }
    return true;
}
}// namespace

QString NotebookStore::newId() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }

QString NotebookStore::typeLabel(const QString &type) {
    if (type == "site") return QCoreApplication::translate("NotebookStore", "静态站点");
    if (type == "ebook") return QCoreApplication::translate("NotebookStore", "电子书");
    if (type == "todo") return QStringLiteral("Todo List");
    return QCoreApplication::translate("NotebookStore", "普通笔记本");
}

QHash<QString, QString> NotebookStore::chapterTitles(const Notebook &book) {
    QHash<QString, QString> titles;
    for (const auto &note: book.notes) titles.insert(note.id, note.title);
    if (book.type != "ebook" || !book.config["autoNumberChapters"].toBool()) return titles;
    QList<QPair<QString, QString>> pending{{QString(), QString()}};
    QSet<QString> visited;
    while (!pending.isEmpty()) {
        const auto entry = pending.takeLast();
        int index = 0;
        for (const auto &note: book.notes) {
            if (note.parentId != entry.first || visited.contains(note.id)) continue;
            visited.insert(note.id);
            const auto number = entry.second + QString::number(++index);
            titles[note.id] = number + " " + note.title;
            pending.append({note.id, number + "."});
        }
    }
    return titles;
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
    if (book.type == "normal") {
        QString directory;
        for (const auto &parent: book.notes) {
            if (parent.id == note.parentId) {
                directory = parent.path;
                break;
            }
        }
        const auto name = note.title == QFileInfo(note.path).fileName() ? note.title : sanitized(note.title);
        return directory.isEmpty() ? name : directory + '/' + name;
    }
    auto name = sanitized(note.title);
    if (name.isEmpty()) name = QStringLiteral("未命名");
    if (!note.folder && !name.endsWith(QStringLiteral(".md"))) name += QStringLiteral(".md");
    if (book.type == "site") {
        // 站点文章目录；分类继续用于组织源文件。
        const auto category = sanitized(note.metadata["category"].toString());
        return (note.metadata["draft"].toBool() ? "source/_drafts/" : "source/_posts/") + (category.isEmpty() ? name : category + "/" + name);
    }
    if (book.type == "todo") {
        // 待办事项按分类目录保存；未分类事项归入内置的「默认」目录。
        auto category = sanitized(note.metadata["category"].toString(QStringLiteral("默认")));
        if (category.isEmpty()) category = QStringLiteral("默认");
        return category + "/" + name;
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

bool NotebookStore::removeNoteFiles(const Notebook &book, const Note &note, QString *error) {
    if (book.path.isEmpty()) return true;
    ArticleRenderer renderer(book.path, book.type == "site" ? "sites" : "notebooks");
    QList<const Note *> pending{&note};
    QSet<QString> removed;
    while (!pending.isEmpty()) {
        const auto *entry = pending.takeLast();
        if (removed.contains(entry->id)) continue;
        removed.insert(entry->id);
        renderer.removeCache(book.type == "site" ? entry->path : entry->id);
        for (const auto &child: book.notes) {
            if (child.parentId == entry->id) pending.append(&child);
        }
    }
    if (note.path.isEmpty()) return true;
    const QDir root(book.path);
    const QString absolute = root.absoluteFilePath(note.path);
    const auto remove = [error](const QString &path, bool directory) {
        if (!QFileInfo::exists(path) && !QFileInfo(path).isSymLink()) return true;
        if (directory ? QDir(path).removeRecursively() : QFile::remove(path)) return true;
        *error = QCoreApplication::translate("NotebookStore", "无法删除：%1").arg(path);
        return false;
    };
    if (!remove(absolute, note.folder)) return false;
    if (book.type == "ebook" && !remove(root.absoluteFilePath(chapterBase(note.path)), true)) return false;
    if (book.type == "normal") return true;
    // 清理变空的分类/章节目录
    auto parent = QFileInfo(absolute).absolutePath();
    while (parent.startsWith(book.path) && parent != book.path && QDir().rmdir(parent)) parent = QFileInfo(parent).absolutePath();
    return true;
}

NotebookStore::RemovalResult NotebookStore::removeNotes(const QString &notebookId, const QSet<QString> &ids) {
    RemovalResult result;
    auto *book = notebook(notebookId);
    if (!book) return result;
    for (const auto &entry: book->notes) {
        if (ids.contains(entry.id)) result.removedIds.insert(entry.id);
    }
    bool changed = true;
    while (changed) {
        changed = false;
        for (const auto &entry: book->notes) {
            if (result.removedIds.contains(entry.parentId) && !result.removedIds.contains(entry.id)) {
                result.removedIds.insert(entry.id);
                changed = true;
            }
        }
    }
    if (result.removedIds.isEmpty()) return result;
    const auto original = *book;
    if (book->type == "normal") {
        bool removed = true;
        for (const auto &entry: original.notes) {
            if (!result.removedIds.contains(entry.id) || result.removedIds.contains(entry.parentId)) continue;
            if (!removeNoteFiles(original, entry, &result.error)) {
                removed = false;
                break;
            }
        }
        QString scanError;
        result.saved = scanDirectory(*book, &scanError) && removed;
        if (result.error.isEmpty()) result.error = scanError;
        for (const auto &entry: book->notes) result.removedIds.remove(entry.id);
        return result;
    }

    for (int index = static_cast<int>(book->notes.size()) - 1; index >= 0; --index) {
        if (result.removedIds.contains(book->notes[index].id)) book->notes.removeAt(index);
    }
    // Persist the intent before touching the original files. A partial save cannot
    // leave an index pointing at a body that this operation has already destroyed.
    if (!save(&result.error)) {
        result.error = QCoreApplication::translate("NotebookStore", "已从当前会话移除所选内容，但保存失败，原文件尚未删除：\n") + result.error;
        return result;
    }
    result.saved = true;
    for (const auto &entry: original.notes) {
        if (!result.removedIds.contains(entry.id) || result.removedIds.contains(entry.parentId)) continue;
        if (!removeNoteFiles(original, entry, &result.error)) {
            result.error = QCoreApplication::translate("NotebookStore", "索引已更新，但部分文件未能删除：\n") + result.error;
            return result;
        }
    }
    return result;
}

NotebookStore::RemovalResult NotebookStore::removeCompletedTodos(const QString &notebookId) {
    const auto *book = notebook(notebookId);
    if (!book || book->type != "todo") return {};
    QSet<QString> ids;
    for (const auto &entry: book->notes) {
        if (!entry.folder && entry.metadata["completed"].toBool()) ids.insert(entry.id);
    }
    return removeNotes(notebookId, ids);
}

bool NotebookStore::importFiles(const QString &notebookId, const QString &parentId, const QStringList &paths, QString *error) {
    auto *book = notebook(notebookId);
    const auto *parent = note(parentId);
    if (!book || book->type != "normal" || (!parentId.isEmpty() && (!parent || !parent->folder || owner(parentId) != book))) {
        *error = QCoreApplication::translate("NotebookStore", "请选择普通笔记本中的目标文件夹。");
        return false;
    }
    const QDir root(book->path);
    const QString directory = root.absoluteFilePath(parent ? parent->path : QString());
    for (const auto &path: paths) {
        const QFileInfo source(QDir::cleanPath(path));
        const auto sourcePath = source.canonicalFilePath();
        const auto targetPath = QFileInfo(directory).canonicalFilePath();
        const auto sourcePrefix = sourcePath.endsWith('/') ? sourcePath : sourcePath + '/';
        if (source.isDir() && (sourcePath == targetPath || targetPath.startsWith(sourcePrefix))) {
            *error = QCoreApplication::translate("NotebookStore", "不能将目录复制到自身或其子目录：%1").arg(path);
            return false;
        }
        auto name = source.fileName();
        const auto taken = [&](const QString &candidate) {
            const QFileInfo existing(QDir(directory).filePath(candidate));
            return existing.exists() || existing.isSymLink();
        };
        for (int index = 2; taken(name); ++index) {
            name = source.isDir() || source.suffix().isEmpty()
                           ? source.fileName() + '-' + QString::number(index)
                           : source.completeBaseName() + '-' + QString::number(index) + '.' + source.suffix();
        }
        const QString destination = QDir(directory).filePath(name);
        QTemporaryDir staging(QDir(directory).filePath(".qsnote-paste-XXXXXX"));
        if (!staging.isValid()) {
            *error = staging.errorString();
            return false;
        }
        struct Entry {
            QString source;
            QString relative;
        };
        QList<Entry> pending{{source.absoluteFilePath(), {}}};
        while (!pending.isEmpty()) {
            const auto entry = pending.takeLast();
            const QFileInfo info(entry.source);
            const auto staged = staging.filePath("content" + entry.relative);
            if (info.isSymLink() || (!info.isDir() && !info.isFile()) || !info.isReadable()) {
                *error = QCoreApplication::translate("NotebookStore", "无法复制文件或目录（不支持符号链接）：%1").arg(entry.source);
                return false;
            }
            if (info.isDir()) {
                if (!QDir().mkpath(staged)) {
                    *error = QCoreApplication::translate("NotebookStore", "无法创建目录：") + staged;
                    return false;
                }
                const auto children = QDir(entry.source).entryInfoList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot, QDir::DirsFirst | QDir::Name);
                for (auto it = children.crbegin(); it != children.crend(); ++it) {
                    pending.append({it->absoluteFilePath(), entry.relative + '/' + it->fileName()});
                }
            } else {
                QFile file(entry.source);
                if (!file.copy(staged)) {
                    *error = entry.source + ": " + file.errorString();
                    return false;
                }
            }
        }
        if (!QDir().rename(staging.filePath("content"), destination)) {
            *error = QCoreApplication::translate("NotebookStore", "无法将 %1 复制到 %2").arg(path, destination);
            return false;
        }
        if (!loadDirectory(*book, root.relativeFilePath(directory) == "." ? QString() : root.relativeFilePath(directory), error)) return false;
    }
    return true;
}

bool NotebookStore::syncNotebook(Notebook &book, QString *error) {
    if (book.path.isEmpty()) {
        *error = QCoreApplication::translate("NotebookStore", "笔记本「%1」未设置保存目录").arg(book.name);
        return false;
    }
    if (book.type == "normal" && !book.notes.isEmpty() && !QDir(book.path).exists()) {
        *error = QCoreApplication::translate("NotebookStore", "无法读取目录：%1").arg(book.path);
        return false;
    }
    if (!QDir().mkpath(book.path)) {
        *error = QCoreApplication::translate("NotebookStore", "无法创建笔记本目录：") + book.path;
        return false;
    }
    if (book.type == "site" && !StaticSite::prepare(book, error)) return false;
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
        if (book.type == "normal" && !oldPath.isEmpty() && !QFileInfo::exists(root.filePath(oldPath)) && !QFileInfo(root.filePath(oldPath)).isSymLink()) {
            if (!note->contentDirty) continue;
            *error = QCoreApplication::translate("NotebookStore", "文件已被移除，未保存的编辑仍保留：%1").arg(oldPath);
            return false;
        }
        // 目标路径与同笔记本其它条目或磁盘残留冲突时追加序号。
        QString expected = relativePath(book, *note);
        const auto taken = [&](const QString &candidate) {
            const QFileInfo existing(root.absoluteFilePath(candidate));
            return used.contains(candidate) || (candidate != oldPath && (existing.exists() || existing.isSymLink()));
        };
        if (taken(expected)) {
            QString suffix = expected.endsWith(QStringLiteral(".md")) ? QStringLiteral(".md") : QString();
            if (book.type == "normal") {
                const QFileInfo info(expected);
                suffix = !note->folder && !info.completeBaseName().isEmpty() && !info.suffix().isEmpty() ? '.' + info.suffix() : QString();
            }
            const QString base = expected.left(expected.size() - suffix.size());
            for (int index = 2;; ++index) {
                const QString candidate = base + "-" + QString::number(index) + suffix;
                if (!taken(candidate)) {
                    expected = candidate;
                    break;
                }
            }
        }
        if (!oldPath.isEmpty() && oldPath != expected) {
            const QString oldAbsolute = root.absoluteFilePath(oldPath);
            const QString newAbsolute = root.absoluteFilePath(expected);
            auto content = note->content;
            if (!note->folder && note->kind == Note::Kind::Markdown && !copyInsertedImages(content, QFileInfo(oldAbsolute).absolutePath(), QFileInfo(newAbsolute).absolutePath(), error)) return false;
            if ((QFileInfo::exists(oldAbsolute) || QFileInfo(oldAbsolute).isSymLink()) && (!QDir().mkpath(QFileInfo(newAbsolute).absolutePath()) || !QDir().rename(oldAbsolute, newAbsolute))) {
                *error = QCoreApplication::translate("NotebookStore", "无法将 %1 移动到 %2").arg(oldAbsolute, newAbsolute);
                return false;
            }
            if (content != note->content) {
                note->content = content;
                note->contentDirty = true;
            }
            // 电子书章节附带同名子章节目录；普通笔记本文件夹本身即目录。
            QString oldPrefix = oldPath + "/";
            QString newPrefix = expected + "/";
            if (book.type == "ebook") {
                const QString oldDirectory = root.absoluteFilePath(chapterBase(oldPath));
                const QString newDirectory = root.absoluteFilePath(chapterBase(expected));
                if (QFileInfo::exists(oldDirectory) && (!QDir().mkpath(QFileInfo(newDirectory).absolutePath()) || !QDir().rename(oldDirectory, newDirectory))) {
                    *error = QCoreApplication::translate("NotebookStore", "无法将 %1 移动到 %2").arg(oldDirectory, newDirectory);
                    return false;
                }
                oldPrefix = chapterBase(oldPath) + "/";
                newPrefix = chapterBase(expected) + "/";
            }
            for (auto &child: book.notes) {
                if (child.path.startsWith(oldPrefix)) child.path = newPrefix + child.path.mid(oldPrefix.size());
            }
            if (book.type == "normal" && note->folder) {
                QSet<QString> directories;
                for (const auto &directory: book.loadedDirectories) {
                    directories.insert(directory == oldPath ? expected : directory.startsWith(oldPrefix) ? newPrefix + directory.mid(oldPrefix.size())
                                                                                                         : directory);
                }
                book.loadedDirectories = directories;
            }
            // 清理移动后变空的目录（如旧分类目录）。
            auto parent = QFileInfo(oldAbsolute).absolutePath();
            while (book.type != "normal" && parent.startsWith(book.path) && parent != book.path && QDir().rmdir(parent)) parent = QFileInfo(parent).absolutePath();
        }
        note->path = expected;
        used.insert(expected);
        if (note->folder) {
            if (!QDir().mkpath(root.absoluteFilePath(expected))) {
                *error = QCoreApplication::translate("NotebookStore", "无法创建目录：") + root.absoluteFilePath(expected);
                return false;
            }
        } else if ((book.type != "normal" || note->kind == Note::Kind::Markdown || note->kind == Note::Kind::Text) && (book.type == "site" || note->contentDirty || !QFileInfo::exists(root.absoluteFilePath(expected)))) {
            auto source = *note;
            if (book.type == "site") TextEncoding::updateText(source.content, QString::fromUtf8(StaticSite::postSource(*note)), source.format);
            QByteArray bytes;
            if (!fileContents(source, bytes, error) || !AtomicFile::write(root.absoluteFilePath(expected), bytes, error)) return false;
            note->contentDirty = false;
            if (book.type == "normal") {
                const QFileInfo saved(root.absoluteFilePath(expected));
                note->metadata["modified"] = saved.lastModified().toMSecsSinceEpoch();
                note->metadata["size"] = saved.size();
                note->metadata["extension"] = saved.suffix();
            }
        }
    }
    if (book.type == "normal") return scanDirectory(book, error);
    QJsonArray entries;
    for (const auto &note: book.notes) {
        entries.append(QJsonObject{{"id", note.id}, {"parentId", note.parentId}, {"title", note.title}, {"folder", note.folder}, {"path", note.path}, {"metadata", note.metadata}});
    }
    // 索引同时记录笔记本元信息，关闭笔记本后仍可凭目录完整恢复。
    const QJsonObject index{{"id", book.id}, {"name", book.name}, {"type", book.type}, {"config", book.config}, {"notes", entries}};
    return AtomicFile::write(root.absoluteFilePath(indexFileName()), QJsonDocument(index).toJson(), error);
}

bool NotebookStore::save(QString *error) {
    for (auto &book: notebooks) {
        if (!syncNotebook(book, error)) return false;
    }
    QJsonArray books;
    for (const auto &book: notebooks) {
        books.append(QJsonObject{{"id", book.id}, {"name", book.name}, {"type", book.type}, {"path", book.path}, {"config", book.config}});
    }
    return AtomicFile::write(storagePath(), QJsonDocument(books).toJson(), error);
}

bool NotebookStore::readFile(Note &note, const QString &path, QString *error, const QByteArray &encoding) {
    const QFileInfo info(path);
    note.kind = Note::Kind::Unsupported;
    note.contentLoaded = true;
    if (info.isSymLink() || !info.isFile()) return true;
    const auto mime = QMimeDatabase().mimeTypeForFile(info);
    if (mime.name().startsWith("image/")) {
        note.kind = Note::Kind::Image;
        return true;
    }
    const bool markdown = info.suffix().compare("md", Qt::CaseInsensitive) == 0 || info.suffix().compare("markdown", Qt::CaseInsensitive) == 0;
    if (!markdown && info.size() != 0 && !mime.inherits("text/plain")) return true;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *error = file.errorString();
        return false;
    }
    const auto bytes = file.readAll();
    if (file.error() != QFileDevice::NoError) {
        *error = file.errorString();
        return false;
    }
    QString content;
    TextEncoding::Format format;
    if (!TextEncoding::decode(bytes, content, format, encoding)) {
        *error = QCoreApplication::translate("NotebookStore", "无法解码文本文件：%1").arg(path);
        return false;
    }
    note.kind = markdown ? Note::Kind::Markdown : Note::Kind::Text;
    note.content = content;
    note.format = format;
    return true;
}

bool NotebookStore::fileContents(const Note &note, QByteArray &bytes, QString *error) {
    if (TextEncoding::encode(note.content, note.format, bytes)) return true;
    *error = QCoreApplication::translate("NotebookStore", "无法使用 %1 编码保存 %2，部分字符无法表示。请切换保存编码为 UTF-8。")
                     .arg(QString::fromLatin1(note.format.name), note.path);
    return false;
}

bool NotebookStore::scanDirectory(Notebook &book, QString *error) {
    const QDir root(book.path);
    if (!root.exists() || !QFileInfo(book.path).isReadable()) {
        *error = QCoreApplication::translate("NotebookStore", "无法读取目录：%1").arg(book.path);
        return false;
    }
    QHash<QString, Note> previous;
    for (const auto &note: book.notes) previous.insert(note.path, note);
    QList<Note> scanned;
    const bool initial = book.loadedDirectories.isEmpty();
    QSet<QString> loaded;
    QList<QPair<QString, QString>> pending{{QString(), QString()}};
    while (!pending.isEmpty()) {
        const auto entry = pending.takeLast();
        const QDir directory(root.filePath(entry.first));
        if (!QFileInfo(directory.path()).isReadable()) continue;
        loaded.insert(entry.first);
        const auto files = directory.entryInfoList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot, QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);
        for (const auto &file: files) {
            const auto path = root.relativeFilePath(file.absoluteFilePath());
            auto note = previous.value(path);
            if (note.id.isEmpty()) {
                note.id = QUrl::fromLocalFile(file.absoluteFilePath()).toString();
                note.contentLoaded = false;
                note.kind = Note::Kind::Unsupported;
            }
            note.path = path;
            note.title = file.fileName();
            note.parentId = entry.second;
            note.folder = file.isDir() && !file.isSymLink();
            if (note.folder) {
                if (book.loadedDirectories.contains(path) || (initial && entry.first.isEmpty())) pending.append({path, note.id});
            } else if (!note.contentDirty && note.contentLoaded && (note.metadata["modified"].toVariant().toLongLong() != file.lastModified().toMSecsSinceEpoch() || note.metadata["size"].toVariant().toLongLong() != file.size() || note.metadata["extension"].toString() != file.suffix())) {
                QString readError;
                note.content.clear();
                note.metadata = {};
                readFile(note, file.absoluteFilePath(), &readError);
                if (!readError.isEmpty()) note.metadata["readError"] = readError;
            }
            note.metadata["modified"] = file.lastModified().toMSecsSinceEpoch();
            note.metadata["size"] = file.size();
            note.metadata["extension"] = file.suffix();
            scanned.append(note);
        }
    }
    // Keep unsaved buffers if an external operation removes their files.
    for (const auto &note: book.notes) {
        if (note.contentDirty && !QFileInfo::exists(root.filePath(note.path))) scanned.append(note);
    }
    book.notes = scanned;
    book.loadedDirectories = loaded;
    return true;
}

bool NotebookStore::loadDirectory(Notebook &book, const QString &path, QString *error) {
    const QFileInfo target(QDir(book.path).filePath(path));
    if (!target.isDir() || !target.isReadable()) {
        *error = QCoreApplication::translate("NotebookStore", "无法读取目录：%1").arg(target.filePath());
        return false;
    }
    auto directory = path;
    while (!directory.isEmpty() && directory != ".") {
        book.loadedDirectories.insert(directory);
        directory = QFileInfo(directory).path();
    }
    book.loadedDirectories.insert(QString());
    return scanDirectory(book, error);
}

bool NotebookStore::loadAllFiles(Notebook &book, QString *error) {
    for (;;) {
        bool unloaded = false;
        for (const auto &note: book.notes) {
            if (note.folder && !book.loadedDirectories.contains(note.path)) {
                if (!QFileInfo(QDir(book.path).filePath(note.path)).isReadable()) continue;
                book.loadedDirectories.insert(note.path);
                unloaded = true;
            }
        }
        if (!unloaded) break;
        if (!scanDirectory(book, error)) return false;
    }
    for (auto &note: book.notes) {
        if (note.folder || note.contentLoaded) continue;
        QString readError;
        readFile(note, QDir(book.path).filePath(note.path), &readError);
    }
    return true;
}

bool NotebookStore::loadNotebook(Notebook &book, QString *error) {
    if (book.path.isEmpty()) return true;
    if (book.type == "normal") return scanDirectory(book, error);
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
        *error = QCoreApplication::translate("NotebookStore", "笔记本「%1」索引格式无效：").arg(book.name) + parseError.errorString();
        return false;
    }
    // 索引是笔记本的权威记录，优先于全局登记信息。
    const auto object = document.object();
    book.id = object["id"].toString(book.id);
    book.name = object["name"].toString(book.name);
    book.type = object["type"].toString(book.type);
    if (object.contains("config")) book.config = object["config"].toObject();
    if (book.type == "site" && book.config["theme"].toString() == "stellar") book.config["theme"] = "next";
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
            if (!content.open(QIODevice::ReadOnly)) {
                *error = content.fileName() + ": " + content.errorString();
                return false;
            }
            const auto bytes = content.readAll();
            if (content.error() != QFileDevice::NoError) {
                *error = content.fileName() + ": " + content.errorString();
                return false;
            }
            if (!TextEncoding::decode(bytes, note.content, note.format)) {
                *error = QCoreApplication::translate("NotebookStore", "无法解码文本文件：%1").arg(content.fileName());
                return false;
            }
            if (book.type == "site") TextEncoding::updateText(note.content, StaticSite::postBody(note.content), note.format);
        }
        loaded.append(note);
    }
    book.notes = loaded;
    return true;
}

bool NotebookStore::readNotebook(const QString &path, Notebook &book, QString *error) {
    book.path = QDir(path).absolutePath();
    book.id = newId();
    book.type = QStringLiteral("normal");
    book.name = QDir(path).dirName();
    QFile index(QDir(path).filePath(indexFileName()));
    if (index.open(QIODevice::ReadOnly)) {
        const auto object = QJsonDocument::fromJson(index.readAll()).object();
        book.id = object["id"].toString(book.id);
        book.name = object["name"].toString(book.name);
        book.type = object["type"].toString(book.type);
        book.config = object["config"].toObject();
    }
    return loadNotebook(book, error);
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
        *error = QCoreApplication::translate("NotebookStore", "笔记数据格式无效：") + parseError.errorString();
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
