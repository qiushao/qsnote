#ifndef QSNOTE_SOURCEOUTLINE_H
#define QSNOTE_SOURCEOUTLINE_H

#include <QString>
#include <QVector>

namespace SourceOutline {
enum class Kind { Namespace,
                  Class,
                  Enum,
                  Function,
                  Method,
                  Variable,
                  Field,
                  Type };

struct Symbol {
    QString name;
    Kind kind = Kind::Variable;
    int line = 0;
    int parent = -1;
};

struct Result {
    bool supported = false;
    QVector<Symbol> symbols;
};

// Parses an editor snapshot; line numbers are zero-based. Safe to run off the UI thread.
Result parse(const QString &fileName, const QString &source);
QString kindName(Kind kind);
}// namespace SourceOutline

#endif
