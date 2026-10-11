#ifndef QSNOTE_EBOOKEXPORTER_H
#define QSNOTE_EBOOKEXPORTER_H

#include "NotebookStore.h"

class EbookExporter {
public:
    enum class Format { Epub, Pdf, Chm, Html };
    // Empty noteId exports the complete notebook; otherwise exports only that document.
    // Creates a new file, adding a numeric suffix if the name already exists.
    static QString exportBook(const Notebook &book, const QString &noteId, Format format,
                              const QString &directory, QString *error);
    static QString chmCompiler();
};

#endif// QSNOTE_EBOOKEXPORTER_H
