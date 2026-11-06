#ifndef QSNOTE_STATICSITE_H
#define QSNOTE_STATICSITE_H

#include "NotebookStore.h"
#include <functional>

class StaticSite {
public:
    using ProgressCallback = std::function<void(int, int, const QString &)>;
    static bool prepare(Notebook &book, QString *error);
    static QByteArray postSource(const Note &note);
    static QString postBody(const QString &source);
    static bool generate(const Notebook &book, QString *error, const ProgressCallback &progress = {});
    static bool copyDirectory(const QString &source, const QString &destination, QString *error);
    static QString rootPath(const Notebook &book);
};

#endif
