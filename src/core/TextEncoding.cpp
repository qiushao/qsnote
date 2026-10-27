#include "TextEncoding.h"
#include <QStringConverter>
#include <QTextCodec>
#include <algorithm>

bool TextEncoding::decode(const QByteArray &bytes, QString &text, Format &format, const QByteArray &encoding) {
    const auto bomEncoding = QStringConverter::encodingForData(bytes);
    const QByteArray bomName = bomEncoding ? QByteArray(QStringConverter::nameForEncoding(*bomEncoding)) : QByteArray();
    if (bomEncoding && !encoding.isEmpty() && encoding != bomName) return false;
    auto name = encoding.isEmpty() ? (bomEncoding ? bomName : QByteArray("UTF-8")) : encoding;
    const auto decodeWith = [&bytes](const QByteArray &codecName, QString &decoded) {
        if (const auto unicode = QStringConverter::encodingForName(codecName.constData())) {
            if ((codecName.startsWith("UTF-16") && bytes.size() % 2 != 0) || (codecName.startsWith("UTF-32") && bytes.size() % 4 != 0)) return false;
            QStringDecoder decoder(*unicode, QStringConverter::Flag::Stateless);
            decoded = decoder(bytes);
            return !decoder.hasError() && !decoded.contains(QChar::Null);
        }
        auto *codec = QTextCodec::codecForName(codecName);
        if (!codec) return false;
        decoded = codec->toUnicode(bytes);
        // Legacy codecs may silently buffer an incomplete trailing character.
        return !decoded.contains(QChar::Null) && codec->fromUnicode(decoded) == bytes;
    };
    QString decoded;
    if (!decodeWith(name, decoded)) {
        if (!encoding.isEmpty() || bomEncoding) return false;
        name = "GBK";
        if (!decodeWith(name, decoded)) return false;
    }
    format.name = name;
    format.bom = bomEncoding && name == bomName;
    format.lineEndings.clear();
    text.clear();
    text.reserve(decoded.size());
    for (qsizetype index = 0; index < decoded.size(); ++index) {
        const auto ch = decoded.at(index);
        if (ch == '\r') {
            const bool crlf = index + 1 < decoded.size() && decoded.at(index + 1) == '\n';
            format.lineEndings.append(crlf ? LineEnding::CRLF : LineEnding::CR);
            if (crlf) ++index;
            text += '\n';
        } else {
            if (ch == '\n') format.lineEndings.append(LineEnding::LF);
            text += ch;
        }
    }
    format.lineEnding = format.lineEndings.isEmpty() ? LineEnding::LF : format.lineEndings.first();
    return true;
}

bool TextEncoding::encode(const QString &text, const Format &format, QByteArray &bytes) {
    auto *codec = QTextCodec::codecForName(format.name);
    if (!codec) return false;
    QString content;
    content.reserve(text.size());
    qsizetype line = 0;
    for (const auto ch: text) {
        if (ch != '\n') {
            content += ch;
            continue;
        }
        const auto ending = format.lineEndings.value(line++, format.lineEnding);
        if (ending != LineEnding::LF) content += '\r';
        if (ending != LineEnding::CR) content += '\n';
    }
    if (format.bom) content.prepend(QChar::ByteOrderMark);
    QTextCodec::ConverterState state(QTextCodec::IgnoreHeader);
    const auto encoded = codec->fromUnicode(content.constData(), static_cast<int>(content.size()), &state);
    if (state.invalidChars != 0 || state.remainingChars != 0) return false;
    bytes = encoded;
    return true;
}

void TextEncoding::updateText(QString &text, const QString &updated, Format &format) {
    if (text == updated) return;
    if (!format.lineEndings.isEmpty()) {
        // Locate the edit so untouched line boundaries retain their original bytes.
        qsizetype prefix = 0;
        const auto commonSize = qMin(text.size(), updated.size());
        while (prefix < commonSize && text.at(prefix) == updated.at(prefix)) ++prefix;
        qsizetype suffix = 0;
        while (suffix < commonSize - prefix && text.at(text.size() - suffix - 1) == updated.at(updated.size() - suffix - 1)) ++suffix;
        const auto first = QStringView(text).first(prefix).count('\n');
        const auto removed = QStringView(text).mid(prefix, text.size() - prefix - suffix).count('\n');
        const auto added = QStringView(updated).mid(prefix, updated.size() - prefix - suffix).count('\n');
        // Keep corresponding boundaries when replacing text across existing lines.
        if (removed > added) format.lineEndings.remove(first + added, removed - added);
        else if (added > removed)
            format.lineEndings.insert(first + removed, added - removed, format.lineEnding);
        if (!format.lineEndings.isEmpty()) format.lineEnding = format.lineEndings.first();
    }
    text = updated;
}

bool TextEncoding::hasMixedLineEndings(const Format &format) {
    return std::any_of(format.lineEndings.cbegin(), format.lineEndings.cend(), [&format](LineEnding ending) { return ending != format.lineEnding; });
}
