#pragma once

#include "NotebookStore.h"
#include <QStringList>

namespace NotebookCategories {
QStringList site(const Notebook &book);
QString slug(const Notebook &book, const QString &name);
bool setSlug(Notebook &book, const QString &name, const QString &value, QString *error);
QStringList todo(const Notebook &book);
// Updates metadata in memory. The caller retains its existing save timing.
bool create(Notebook &book, const QString &name, QString *error);
bool rename(Notebook &book, const QString &previous, const QString &name, QString *error);
bool remove(Notebook &book, const QString &name, QString *error);
}// namespace NotebookCategories
