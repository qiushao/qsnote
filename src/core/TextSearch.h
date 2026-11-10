#pragma once

#include <QRegularExpression>

namespace TextSearch {
enum class Mode { Normal, Extended, Regex };
struct Options {
    Mode mode = Mode::Normal;
    bool caseSensitive = false;
    bool wholeWord = false;
    bool dotMatchesNewline = false;
};
QString decodeEscapes(const QString &text);
QRegularExpression expression(const QString &query, const Options &options);
QString replacement(const QString &text, Mode mode, const QRegularExpressionMatch &match);
}// namespace TextSearch
