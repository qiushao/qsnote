#include "core/FrontMatter.h"
#include "core/StaticSite.h"
#include <QtWidgets>

namespace {
void require(bool ok, const QString &message) {
    if (!ok) qFatal("%s", qPrintable(message));
}
void write(const QString &path, const QByteArray &data) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    require(file.open(QIODevice::WriteOnly) && file.write(data) == data.size(), "fixture write");
}
QByteArray read(const QString &path) {
    QFile file(path);
    require(file.open(QIODevice::ReadOnly), "fixture read");
    return file.readAll();
}
}// namespace
int main(int argc, char **argv) {
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QApplication app(argc, argv);
    QTemporaryDir temp;
    qputenv("XDG_CACHE_HOME", temp.path().toUtf8());
    const QList<QPair<QString, QJsonObject>> valid = {
            {"plain\r\nbody", {}},
            {"---\n{\"title\":\"JSON\",\"draft\":false,\"tags\":[\"a\",\"b\"]}\n---\nbody", {{"title", "JSON"}, {"draft", false}, {"tags", QJsonArray{"a", "b"}}}},
            {"---\r\ntitle: 'Author''s title'\r\ncomments: false\r\ndraft: true\r\ncount: 42\r\ntags: [one, 'two']\r\ncategories:\r\n  - A\r\n  - \"B\"\r\n# comment\r\n---\r\nbody", {{"title", "Author's title"}, {"comments", false}, {"draft", true}, {"count", "42"}, {"tags", QJsonArray{"one", "two"}}, {"categories", QJsonArray{"A", "B"}}}},
            {"---\n{}\n---", {}}};
    for (const auto &entry: valid) {
        auto body = entry.first;
        QJsonObject metadata;
        QString error;
        require(FrontMatter::parse(body, metadata, &error) && error.isEmpty(), error);
        require(metadata == entry.second, "metadata values and supported scalar semantics");
        require(body == (entry.first.startsWith("plain") ? "plain\nbody" : entry.first.endsWith("body") ? "body"
                                                                                                        : ""),
                "separate body and normalize CRLF");
    }
    for (const auto &source: {QString("---\n{broken}\n---\nbody"), QString("---\ntitle: title\nbody"), QString("---\ntitle: title\n---trailing\nbody"), QString("---\nnested:\n  child: value\n---\nbody"), QString("---\ntitle:no-space\n---\nbody")}) {
        auto body = source;
        QJsonObject metadata;
        QString error;
        require(!FrontMatter::parse(body, metadata, &error) && !error.isEmpty() && body == source, "invalid source reports error without stripping body");
    }
    const QString authored = "---\ntitle: Author\n---\nAuthor body";
    Note note{"post", {}, "Generated title", authored, false, {{"slug", "post"}, {"date", "2026-10-24"}}};
    require(QString::fromUtf8(StaticSite::postSource(note)) == authored && StaticSite::postBody(authored) == authored, "storage preserves author front matter");
    auto parsed = authored;
    QJsonObject metadata;
    QString error;
    require(FrontMatter::parse(parsed, metadata, &error) && parsed == "Author body" && metadata["title"] == "Author", "parser consumes authored header");
    note.content = "Generated body";
    const auto generated = QString::fromUtf8(StaticSite::postSource(note));
    require(StaticSite::postBody(generated) == note.content, "storage strips QSNote generated header");
    parsed = generated;
    metadata = {};
    require(FrontMatter::parse(parsed, metadata, &error) && parsed == note.content && metadata["title"] == "Generated title", "parser accepts generated JSON header");
    Notebook book{"site", "Site", "site", {}, {{"domain", "https://example.com/blog/"}}, temp.filePath("site")};
    require(StaticSite::prepare(book, &error), error);
    write(book.path + "/source/_posts/post.md", "---\ntitle: Authored page\nslug: authored\ndate: 2026-10-24\ncategories: [Cat]\ntags: [Tag]\n---\n# Heading\nAuthor body");
    require(StaticSite::generate(book, &error), error);
    const auto html = read(book.path + "/public/authored/index.html");
    require(html.contains("Authored page") && html.contains("Author body") && !html.contains("slug: authored"), "generation uses parsed title route and body");
    write(book.path + "/source/_posts/post.md", "---\n{broken}\n---\nbody");
    require(!StaticSite::generate(book, &error) && error.contains("post.md") && error.contains("JSON"), "generation preserves source path and parse error");
    require(read(book.path + "/public/authored/index.html") == html, "invalid front matter leaves previous output intact");
    qInfo() << "PASS: no header, JSON/simple YAML/CRLF/scalars, malformed input, generated header separation, generation and failure preservation";
}
