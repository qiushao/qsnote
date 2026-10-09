#include "NotebookStore.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>

QString NotebookStore::newId() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }

QString NotebookStore::typeLabel(const QString &type) {
    if (type == "site") return QStringLiteral("静态站点");
    if (type == "ebook") return QStringLiteral("电子书");
    return QStringLiteral("普通笔记本");
}

QString NotebookStore::storagePath() {
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/notebooks.json";
}

bool NotebookStore::load(QString *error) {
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
    notebooks.clear();
    for (const auto &value: document.array()) {
        const auto object = value.toObject();
        Notebook book;
        book.id = object["id"].toString();
        book.name = object["name"].toString();
        book.type = object["type"].toString();
        book.config = object["config"].toObject();
        for (const auto &entry: object["notes"].toArray()) {
            const auto item = entry.toObject();
            book.notes.append({item["id"].toString(), item["parentId"].toString(), item["title"].toString(), item["content"].toString(), item["folder"].toBool(), item["metadata"].toObject()});
        }
        notebooks.append(book);
    }
    return true;
}

bool NotebookStore::save(QString *error) const {
    QJsonArray books;
    for (const auto &book: notebooks) {
        QJsonArray notes;
        for (const auto &note: book.notes) {
            notes.append(QJsonObject{{"id", note.id}, {"parentId", note.parentId}, {"title", note.title}, {"content", note.content}, {"folder", note.folder}, {"metadata", note.metadata}});
        }
        books.append(QJsonObject{{"id", book.id}, {"name", book.name}, {"type", book.type}, {"config", book.config}, {"notes", notes}});
    }
    if (!QDir().mkpath(QFileInfo(storagePath()).absolutePath())) {
        *error = QStringLiteral("无法创建笔记目录");
        return false;
    }
    QSaveFile file(storagePath());
    if (!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(books).toJson()) == -1 || !file.commit()) {
        *error = file.errorString();
        return false;
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
