#include "TextEncoding.h"
#include <QStringConverter>
#include <QTextCodec>

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
    format.crlf = decoded.contains("\r\n");
    text = decoded.replace("\r\n", "\n");
    return true;
}

bool TextEncoding::encode(const QString &text, const Format &format, QByteArray &bytes) {
    auto *codec = QTextCodec::codecForName(format.name);
    if (!codec) return false;
    auto content = text;
    if (format.crlf) content.replace("\n", "\r\n");
    if (format.bom) content.prepend(QChar::ByteOrderMark);
    QTextCodec::ConverterState state(QTextCodec::IgnoreHeader);
    const auto encoded = codec->fromUnicode(content.constData(), static_cast<int>(content.size()), &state);
    if (state.invalidChars != 0 || state.remainingChars != 0) return false;
    bytes = encoded;
    return true;
}
