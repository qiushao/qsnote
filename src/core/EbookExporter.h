#ifndef QSNOTE_EBOOKEXPORTER_H
#define QSNOTE_EBOOKEXPORTER_H

#include "NotebookStore.h"
#include <functional>

class EbookExporter {
public:
    enum class Format { Epub, Pdf, Chm, Html };
    using ProgressCallback = std::function<void(int completed, int total, const QString &message)>;
    // Empty noteId exports the complete notebook; otherwise exports only that document.
    // Creates a new file, adding a numeric suffix if the name already exists.
    static QString exportBook(const Notebook &book, const QString &noteId, Format format,
                              const QString &directory, QString *error, const ProgressCallback &progress = {});
    static QString chmCompiler();
};

#endif// QSNOTE_EBOOKEXPORTER_H
