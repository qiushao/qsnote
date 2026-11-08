#pragma once

#include "NotebookStore.h"
#include <QJsonArray>
#include <QMap>

// config.docCollections: ordered {id, name, slug} objects.
// metadata.contentType == "doc" and metadata.collectionId identify documents;
// parentId and the notebook note order define their tree.
namespace SiteDocuments {
bool isDocument(const Note &note);
bool validSlug(const QString &slug);
const Note *find(const Notebook &book, const QString &id);
QJsonObject collection(const Notebook &book, const QString &id);
QString sourcePath(const Notebook &book, const Note &note);
QMap<QString, QString> collectionMoves(const Notebook &book);
QString route(const Notebook &book, const Note &note);
QList<const Note *> ordered(const Notebook &book, const QString &collectionId);
bool validate(const Notebook &book, QString *error);
bool validateNavigation(const Notebook &book, QString *error);
bool move(Notebook &book, const QString &id, const QString &parentId, QString *error);
bool reorder(Notebook &book, const QString &id, int offset);
bool updateLinks(Notebook &book, QString *error);
}
