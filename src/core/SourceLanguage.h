#ifndef QSNOTE_SOURCELANGUAGE_H
#define QSNOTE_SOURCELANGUAGE_H

#include <QString>

struct TSLanguage;

namespace SourceLanguage {
struct Definition {
    const TSLanguage *grammar = nullptr;
    QString name;
};
Definition forFile(const QString &fileName);
}// namespace SourceLanguage

#endif
