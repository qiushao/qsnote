#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>

namespace TextEncoding {
enum class LineEnding { LF,
                        CRLF,
                        CR };

struct Format {
    QByteArray name = "UTF-8";
    bool bom = false;
    LineEnding lineEnding = LineEnding::LF;
    QVector<LineEnding> lineEndings;
};

// An empty encoding selects BOM detection, then strict UTF-8, then GBK.
bool decode(const QByteArray &bytes, QString &text, Format &format, const QByteArray &encoding = {});
bool encode(const QString &text, const Format &format, QByteArray &bytes);
// Update LF-normalized editor text while retaining existing line endings.
void updateText(QString &text, const QString &updated, Format &format);
bool hasMixedLineEndings(const Format &format);
}// namespace TextEncoding
