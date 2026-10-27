#pragma once

#include <QByteArray>
#include <QString>

namespace TextEncoding {
struct Format {
    QByteArray name = "UTF-8";
    bool bom = false;
    bool crlf = false;
};

// An empty encoding selects BOM detection, then strict UTF-8, then GBK.
bool decode(const QByteArray &bytes, QString &text, Format &format, const QByteArray &encoding = {});
bool encode(const QString &text, const Format &format, QByteArray &bytes);
}// namespace TextEncoding
