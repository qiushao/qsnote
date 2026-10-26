#ifndef QSNOTE_NOTEBOOKSTORE_H
#define QSNOTE_NOTEBOOKSTORE_H

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

struct Note {
    enum class Kind { Markdown,
                      Text,
                      Image,
                      Unsupported };
    QString id;
    QString parentId;
    QString title;
    QString content;
    bool folder = false;
    QJsonObject metadata;
    QString path;               // 相对笔记本根目录的路径；为空表示保存时再分配
    bool contentDirty = false;  // 内容自上次写入磁盘后被修改
    Kind kind = Kind::Markdown;
    bool contentLoaded = true;
};

struct Notebook {
    QString id;
    QString name;
    QString type;
    QList<Note> notes;
    QJsonObject config;
    QString path;  // 笔记本根目录绝对路径，创建时由用户选择
    QSet<QString> loadedDirectories;// 普通笔记本已枚举的相对目录路径，仅用于当前会话。
};

// 全局 notebooks.json 只保存笔记本基本信息（id/name/type/path/config）。
// 普通笔记本直接读取磁盘目录；其它类型的 _index.json 保存笔记结构（不含内容），
// 普通笔记本保留原始文件名；电子书按层级保存 md，静态站点按分类保存带 JSON front matter 的 md。
class NotebookStore {
public:
    struct RemovalResult {
        QSet<QString> removedIds; // Removed from memory; UI must close these documents.
        bool saved = false;       // Indexed notebooks save their indexes before deleting files.
        QString error;            // On failure, explains whether saving or file cleanup failed.
    };
    QList<Notebook> notebooks;
    bool load(QString *error);
    bool save(QString *error);
    Notebook *notebook(const QString &id);
    Note *note(const QString &id);
    Notebook *owner(const QString &noteId);
    static QString newId();
    static QString typeLabel(const QString &type);
    static QHash<QString, QString> chapterTitles(const Notebook &book);
    static QString storagePath();
    static QString indexFileName();
    static QString sanitized(const QString &name);
    static QString defaultRoot();
    static QString uniqueDirectory(const QString &root, const QString &name);
    RemovalResult removeNotes(const QString &notebookId, const QSet<QString> &ids);
    RemovalResult removeCompletedTodos(const QString &notebookId);
    bool importFiles(const QString &notebookId, const QString &parentId, const QStringList &paths, QString *error);
    static bool scanDirectory(Notebook &book, QString *error);
    static bool loadDirectory(Notebook &book, const QString &path, QString *error);
    static bool loadAllFiles(Notebook &book, QString *error);
    static bool readFile(Note &note, const QString &path, QString *error);
    static QByteArray fileContents(const Note &note);
    static bool readNotebook(const QString &path, Notebook &book, QString *error);

private:
    static bool removeNoteFiles(const Notebook &book, const Note &note, QString *error);
    static bool loadNotebook(Notebook &book, QString *error);
    static bool syncNotebook(Notebook &book, QString *error);
    static QString chapterBase(const QString &path);
    static QString relativePath(const Notebook &book, const Note &note);
};

#endif// QSNOTE_NOTEBOOKSTORE_H
