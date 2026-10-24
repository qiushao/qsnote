#include "MathStylesheet.h"
#include <QFile>
#include <QRegularExpression>

namespace {
bool readResource(const QString &path, QByteArray &bytes, QString *error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *error = path + ": " + file.errorString();
        return false;
    }
    bytes = file.readAll();
    if (file.error() != QFileDevice::NoError) {
        *error = path + ": " + file.errorString();
        return false;
    }
    return true;
}
}// namespace

QString embeddedMathStylesheet(QString *error) {
    // Cache only complete results; a failed resource read must never become valid CSS.
    static QString cached;
    if (!cached.isEmpty()) return cached;
    QByteArray bytes;
    if (!readResource(":/web/vendor/katex/katex.min.css", bytes, error)) return {};
    const auto css = QString::fromUtf8(bytes);
    const QRegularExpression sources(R"(src:url\(([^)]+\.woff2)\)[^}]+)");
    auto matches = sources.globalMatch(css);
    QString embedded;
    qsizetype offset = 0;
    while (matches.hasNext()) {
        const auto match = matches.next();
        if (!readResource(":/web/vendor/katex/" + match.captured(1), bytes, error)) return {};
        embedded += css.mid(offset, match.capturedStart() - offset);
        embedded += "src:url(data:font/woff2;base64," + QString::fromLatin1(bytes.toBase64()) + ") format(\"woff2\")";
        offset = match.capturedEnd();
    }
    cached = embedded + css.mid(offset);
    return cached;
}
