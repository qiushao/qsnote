#include "core/TextEncoding.h"
#include <QCoreApplication>
#include <QDebug>
#include <cstdlib>

namespace {
using TextEncoding::LineEnding;
void check(bool condition, const char *message) {
    if (condition) return;
    qCritical() << message;
    std::exit(1);
}

void roundTrip(const QByteArray &bytes, const QByteArray &encoding, bool bom, LineEnding lineEnding, const QString &expected) {
    TextEncoding::Format format;
    QString text;
    check(TextEncoding::decode(bytes, text, format), "decode failed");
    check(text == expected, "decoded text differs");
    check(format.name == encoding && format.bom == bom && format.lineEnding == lineEnding, "detected format differs");
    QByteArray saved;
    check(TextEncoding::encode(text, format, saved), "encode failed");
    if (saved != bytes) qCritical() << encoding << "expected" << bytes.toHex() << "got" << saved.toHex();
    check(saved == bytes, "round trip changed bytes");
}
}// namespace

int main(int argc, char **argv) {
    const QCoreApplication app(argc, argv);
    const auto chinese = QStringLiteral("中文测试\n");
    roundTrip(chinese.toUtf8(), "UTF-8", false, LineEnding::LF, chinese);
    roundTrip(QByteArray::fromHex("efbbbf") + chinese.toUtf8(), "UTF-8", true, LineEnding::LF, chinese);
    roundTrip(QByteArray::fromHex("d6d0cec4b2e2cad40d0a"), "GBK", false, LineEnding::CRLF, chinese);
    roundTrip({}, "UTF-8", false, LineEnding::LF, {});
    roundTrip("ASCII\r\n", "UTF-8", false, LineEnding::CRLF, "ASCII\n");
    roundTrip("first\nsecond\r\n", "UTF-8", false, LineEnding::LF, "first\nsecond\n");
    roundTrip("first\rsecond\r", "UTF-8", false, LineEnding::CR, "first\nsecond\n");
    roundTrip("a\r\nb\nc\rd", "UTF-8", false, LineEnding::CRLF, "a\nb\nc\nd");
    roundTrip("no newline", "UTF-8", false, LineEnding::LF, "no newline");
    roundTrip(QByteArray::fromHex("efbbbf"), "UTF-8", true, LineEnding::LF, {});
    roundTrip(QByteArray::fromHex("fffe2d4e87650d000a00"), "UTF-16LE", true, LineEnding::CRLF, QStringLiteral("中文\n"));
    roundTrip(QByteArray::fromHex("feff4e2d6587000a"), "UTF-16BE", true, LineEnding::LF, QStringLiteral("中文\n"));
    roundTrip(QByteArray::fromHex("fffe00002d4e0000876500000a000000"), "UTF-32LE", true, LineEnding::LF, QStringLiteral("中文\n"));
    roundTrip(QByteArray::fromHex("0000feff00004e2d000065870000000a"), "UTF-32BE", true, LineEnding::LF, QStringLiteral("中文\n"));

    TextEncoding::Format format;
    QString text;
    check(TextEncoding::decode(QByteArray::fromHex("c2a9"), text, format) && format.name == "UTF-8", "valid UTF-8 must win ambiguous detection");
    check(TextEncoding::decode(QByteArray::fromHex("c2a9"), text, format, "GBK") && text == QStringLiteral("漏"), "manual GBK decoding failed");
    check(!TextEncoding::decode(QByteArray::fromHex("d6d0"), text, format, "UTF-8"), "invalid forced UTF-8 accepted");
    check(!TextEncoding::decode(QByteArray::fromHex("81"), text, format), "truncated GBK accepted");
    check(!TextEncoding::decode(QByteArray::fromHex("feff4e2d"), text, format, "UTF-16LE"), "conflicting BOM silently changed selected encoding");
    check(!TextEncoding::decode(QByteArray::fromHex("fffe2d"), text, format), "truncated UTF-16 accepted");
    check(!TextEncoding::decode(QByteArray::fromHex("efbbbfff"), text, format), "invalid BOM-marked UTF-8 accepted");
    check(!TextEncoding::decode(QByteArray::fromHex("610062"), text, format), "binary NUL accepted");

    QByteArray saved = "unchanged";
    format = {"GBK", false, LineEnding::LF, {}};
    check(!TextEncoding::encode(QString::fromUtf8("中文😀"), format, saved), "unrepresentable GBK characters accepted");
    check(saved == "unchanged", "failed conversion modified output");
    format.name = "UTF-8";
    check(TextEncoding::encode(QString::fromUtf8("中文😀"), format, saved), "UTF-8 emoji conversion failed");
    check(saved == QString::fromUtf8("中文😀").toUtf8(), "UTF-8 emoji conversion corrupted text");
    check(TextEncoding::decode("a\r\nb\nc\rd", text, format), "mixed decode failed");
    check(TextEncoding::hasMixedLineEndings(format), "mixed line endings not detected");
    TextEncoding::updateText(text, "aa\nb\nc\nd", format);
    check(TextEncoding::encode(text, format, saved) && saved == "aa\r\nb\nc\rd", "editing changed untouched line endings");
    TextEncoding::updateText(text, "aa\nb\nnew\nc\nd", format);
    check(TextEncoding::encode(text, format, saved) && saved == "aa\r\nb\nnew\r\nc\rd", "new line did not use original default");
    TextEncoding::updateText(text, "aa\nb\nc\nd", format);
    check(TextEncoding::encode(text, format, saved) && saved == "aa\r\nb\nc\rd", "deleting a line changed remaining line endings");
    TextEncoding::updateText(text, "AA\nB\nc\nd", format);
    check(TextEncoding::encode(text, format, saved) && saved == "AA\r\nB\nc\rd", "multiline replacement changed existing boundaries");
    for (const auto ending: {LineEnding::LF, LineEnding::CRLF, LineEnding::CR}) {
        format.lineEnding = ending;
        format.lineEndings.clear();
        check(!TextEncoding::hasMixedLineEndings(format), "explicit conversion still marked mixed");
        const auto separator = ending == LineEnding::LF ? "\n" : ending == LineEnding::CRLF ? "\r\n"
                                                                                            : "\r";
        const auto expected = QByteArray("AA") + separator + "B" + separator + "c" + separator + "d";
        check(TextEncoding::encode(text, format, saved) && saved == expected, "explicit line ending conversion failed");
        QString reopened;
        TextEncoding::Format detected;
        check(TextEncoding::decode(saved, reopened, detected) && reopened == text && detected.lineEnding == ending, "converted file did not reopen correctly");
        TextEncoding::updateText(reopened, "more\n" + reopened, detected);
        check(TextEncoding::encode(reopened, detected, saved) && saved == QByteArray("more") + separator + expected, "editing converted file changed line endings");
    }
    check(TextEncoding::decode("a\r\nb\n", text, format), "mixed decode failed");
    TextEncoding::updateText(text, "b\n", format);
    check(!TextEncoding::hasMixedLineEndings(format) && format.lineEnding == LineEnding::LF, "deleting first line left stale format");
    check(TextEncoding::decode("a\rb\r", text, format), "CR decode failed");
    TextEncoding::updateText(text, {}, format);
    TextEncoding::updateText(text, "new\n", format);
    check(TextEncoding::encode(text, format, saved) && saved == "new\r", "clearing text lost selected line ending");
    check(TextEncoding::decode("---\r\nheader\r\n---\r\nbody\nend\r", text, format), "site decode failed");
    TextEncoding::updateText(text, "body\nend\n", format);
    check(TextEncoding::encode(text, format, saved) && saved == "body\nend\r", "removing site header changed body endings");
    TextEncoding::updateText(text, "---\nheader\n---\n" + text, format);
    check(TextEncoding::encode(text, format, saved) && saved == "---\nheader\n---\nbody\nend\r", "adding site header changed body endings");
    qInfo() << "Text encoding tests passed";
}
