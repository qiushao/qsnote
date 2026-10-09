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
};

struct Notebook {
    QString id;
    QString name;
    QString type;
    QList<Note> notes;
    QJsonObject config;
};

class NotebookStore {
public:
    QList<Notebook> notebooks;
    bool load(QString *error);
    bool save(QString *error) const;
    Notebook *notebook(const QString &id);
    Note *note(const QString &id);
    Notebook *owner(const QString &noteId);
    static QString newId();
    static QString typeLabel(const QString &type);
    static QString storagePath();
};

#endif// QSNOTE_NOTEBOOKSTORE_H
