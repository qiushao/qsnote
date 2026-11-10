#include "TextSearch.h"

QString TextSearch::decodeEscapes(const QString &text) {
    QString result;
    for (qsizetype index = 0; index < text.size(); ++index) {
        if (text[index] == '\\' && index + 1 < text.size()) {
            const auto next = text[index + 1];
            if (next == 'n') result += '\n';
            else if (next == 'r') result += '\r';
            else if (next == 't') result += '\t';
            else if (next == '0') result += QChar(0);
            else if (next == '\\') result += '\\';
            else {
                result += text[index];
                continue;
            }
            ++index;
        } else result += text[index];
    }
    return result;
}

QRegularExpression TextSearch::expression(const QString &query, const Options &options) {
    auto pattern = options.mode == Mode::Regex ? query : QRegularExpression::escape(options.mode == Mode::Extended ? decodeEscapes(query) : query);
    if (options.wholeWord) pattern = "(?<!\\w)(?:" + pattern + ")(?!\\w)";
    auto flags = QRegularExpression::UseUnicodePropertiesOption | QRegularExpression::MultilineOption;
    if (!options.caseSensitive) flags |= QRegularExpression::CaseInsensitiveOption;
    if (options.dotMatchesNewline) flags |= QRegularExpression::DotMatchesEverythingOption;
    return QRegularExpression(pattern, flags);
}

QString TextSearch::replacement(const QString &text, Mode mode, const QRegularExpressionMatch &match) {
    if (mode == Mode::Normal) return text;
    if (mode == Mode::Extended) return decodeEscapes(text);
    QString result;
    for (qsizetype index = 0; index < text.size(); ++index) {
        const auto current = text[index];
        if ((current == '\\' || current == '$') && index + 1 < text.size()) {
            const auto next = text[index + 1];
            if (next >= '0' && next <= '9') {
                qsizetype end = index + 1;
                while (end < text.size() && text[end] >= '0' && text[end] <= '9') ++end;
                result += match.captured(text.mid(index + 1, end - index - 1).toInt());
                index = end - 1;
                continue;
            }
            if (current == '$' && next == '{') {
                const auto end = text.indexOf('}', index + 2);
                if (end >= 0) {
                    result += match.captured(text.mid(index + 2, end - index - 2));
                    index = end;
                    continue;
                }
            }
            if (current == '\\') {
                result += decodeEscapes(text.mid(index, 2));
                ++index;
                continue;
            }
            if (next == '$') {
                result += '$';
                ++index;
                continue;
            }
        }
        result += current;
    }
    return result;
}
