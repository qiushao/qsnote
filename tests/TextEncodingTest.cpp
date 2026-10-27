#include "core/TextEncoding.h"
#include <QCoreApplication>
#include <QDebug>
#include <cstdlib>

namespace {
void check(bool condition, const char *message) {
    if (condition) return;
    qCritical() << message;
    std::exit(1);
}

void roundTrip(const QByteArray &bytes, const QByteArray &encoding, bool bom, bool crlf, const QString &expected) {
    TextEncoding::Format format;
    QString text;
    check(TextEncoding::decode(bytes, text, format), "decode failed");
    check(text == expected, "decoded text differs");
    check(format.name == encoding && format.bom == bom && format.crlf == crlf, "detected format differs");
    QByteArray saved;
    check(TextEncoding::encode(text, format, saved), "encode failed");
    if (saved != bytes) qCritical() << encoding << "expected" << bytes.toHex() << "got" << saved.toHex();
    check(saved == bytes, "round trip changed bytes");
}
}// namespace

int main(int argc, char **argv) {
    const QCoreApplication app(argc, argv);
    const auto chinese = QStringLiteral("中文测试\n");
    roundTrip(chinese.toUtf8(), "UTF-8", false, false, chinese);
    roundTrip(QByteArray::fromHex("efbbbf") + chinese.toUtf8(), "UTF-8", true, false, chinese);
    roundTrip(QByteArray::fromHex("d6d0cec4b2e2cad40d0a"), "GBK", false, true, chinese);
    roundTrip({}, "UTF-8", false, false, {});
    roundTrip("ASCII\r\n", "UTF-8", false, true, "ASCII\n");
    roundTrip(QByteArray::fromHex("efbbbf"), "UTF-8", true, false, {});
    roundTrip(QByteArray::fromHex("fffe2d4e87650d000a00"), "UTF-16LE", true, true, QStringLiteral("中文\n"));
    roundTrip(QByteArray::fromHex("feff4e2d6587000a"), "UTF-16BE", true, false, QStringLiteral("中文\n"));
    roundTrip(QByteArray::fromHex("fffe00002d4e0000876500000a000000"), "UTF-32LE", true, false, QStringLiteral("中文\n"));
    roundTrip(QByteArray::fromHex("0000feff00004e2d000065870000000a"), "UTF-32BE", true, false, QStringLiteral("中文\n"));

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
    format = {"GBK", false, false};
    check(!TextEncoding::encode(QString::fromUtf8("中文😀"), format, saved), "unrepresentable GBK characters accepted");
    check(saved == "unchanged", "failed conversion modified output");
    format.name = "UTF-8";
    check(TextEncoding::encode(QString::fromUtf8("中文😀"), format, saved), "UTF-8 emoji conversion failed");
    check(saved == QString::fromUtf8("中文😀").toUtf8(), "UTF-8 emoji conversion corrupted text");
    qInfo() << "Text encoding tests passed";
}
