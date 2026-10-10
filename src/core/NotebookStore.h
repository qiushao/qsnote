#ifndef QSNOTE_NOTEBOOKSTORE_H
#define QSNOTE_NOTEBOOKSTORE_H

#include <QJsonObject>
#include <QList>
#include <QString>

struct Note {
    QString id;
    QString parentId;
    QString title;
    QString content;
    bool folder = false;
    QJsonObject metadata;
    QString path;               // 相对笔记本根目录的路径；为空表示保存时再分配
    bool contentDirty = false;  // 内容自上次写入磁盘后被修改
};

struct Notebook {
    QString id;
    QString name;
    QString type;
    QList<Note> notes;
    QJsonObject config;
    QString path;  // 笔记本根目录绝对路径，创建时由用户选择
};

// 全局 notebooks.json 只保存笔记本基本信息（id/name/type/path/config）。
// 每个笔记本目录下的 _index.json 保存笔记结构（含相对路径，不含内容），
// 文章内容按类型落盘：普通/电子书按目录层级保存 md，静态站点按分类建目录保存 md。
class NotebookStore {
public:
    QList<Notebook> notebooks;
    bool load(QString *error);
    bool save(QString *error);
    Notebook *notebook(const QString &id);
    Note *note(const QString &id);
    Notebook *owner(const QString &noteId);
    static QString newId();
    static QString typeLabel(const QString &type);
    static QString storagePath();
    static QString indexFileName();
    static QString sanitized(const QString &name);
    static QString defaultRoot();
    static QString uniqueDirectory(const QString &root, const QString &name);
    static void removeNoteFiles(const Notebook &book, const Note &note);
    static bool readNotebook(const QString &path, Notebook &book, QString *error);

private:
    static bool loadNotebook(Notebook &book, QString *error);
    static bool syncNotebook(Notebook &book, QString *error);
    static QString chapterBase(const QString &path);
    static QString relativePath(const Notebook &book, const Note &note);
};

#endif// QSNOTE_NOTEBOOKSTORE_H
