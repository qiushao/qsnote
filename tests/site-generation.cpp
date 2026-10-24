#include "core/StaticSite.h"
#include <QtWidgets>

namespace {
void require(bool ok, const QString &message) {
    if (!ok) qFatal("%s", qPrintable(message));
}
void write(const QString &path, const QByteArray &bytes) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    require(file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size(), "fixture write: " + path);
}
QByteArray read(const QString &path) {
    QFile file(path);
    require(file.open(QIODevice::ReadOnly), "read: " + path);
    return file.readAll();
}
QString hash(const QByteArray &data) { return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex()); }
QJsonObject manifest(const QString &root) {
    QStringList files;
    QDirIterator iterator(root, QDir::Files, QDirIterator::Subdirectories);
    while (iterator.hasNext()) files.append(QDir(root).relativeFilePath(iterator.next()));
    files.sort();
    QByteArray all;
    QJsonObject result;
    for (const auto &file: files) {
        const auto digest = hash(read(root + '/' + file));
        all += file.toUtf8() + '\0' + digest.toUtf8() + '\n';
        if (file.endsWith(".html") || file == "search.json" || file == "content.json") result[file] = digest;
    }
    result["all-files"] = hash(all);
    result["file-count"] = files.size();
    return result;
}
void fixedDates(const QString &root) {
    QDirIterator iterator(root, QDir::Files, QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        QFile file(iterator.next());
        require(file.open(QIODevice::ReadWrite), "timestamp fixture");
        require(file.setFileTime(QDateTime::fromString("2026-10-24T12:00:00Z", Qt::ISODate), QFileDevice::FileModificationTime), "fixed timestamp");
    }
}
}// namespace
int main(int argc, char **argv) {
    // QDom attribute order uses hashing; fix its seed for byte-for-byte comparisons.
    QHashSeed::setDeterministicGlobalSeed();
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QApplication app(argc, argv);
    QTemporaryDir temp;
    require(temp.isValid(), "temporary directory");
    qputenv("XDG_CACHE_HOME", temp.path().toUtf8());
    QJsonObject snapshots;
    QString error;
    for (const auto &theme: {QString("pure"), QString("next"), QString("yilia"), QString("3-jekyll"), QString("custom")}) {
        Notebook book{theme, "Site", "site", {}, {{"theme", theme}, {"domain", "https://example.com/blog/"}, {"title", "Site <&>"}, {"author", "Author {{title}} <&>"}, {"city", "City &"}, {"github", "https://example.com/social?a=1&b=2"}, {"description", "Description <&>"}, {"keywords", "first，second"}, {"footer", "Footer <&>"}, {"customHeadCode", "<meta name=\"test-head\" content=\"head\">"}, {"customBodyCode", "<div id=\"test-body\">body</div>"}, {"analyticsCode", "<script id=\"test-analytics\">void 0;</script>"}, {"links", QJsonArray{QJsonObject{{"name", "Friend <&>"}, {"url", "https://example.com/friend?a=1&b=2"}}, QJsonObject{{"name", "Bad friend"}, {"url", "javascript:alert(1)"}}}}}, temp.filePath(theme)};
        const auto platform = theme == "next" ? "gitalk" : theme == "yilia" ? "disqus"
                                                                            : "valine";
        book.config["commentPlatform"] = platform;
        book.config["valineAppId"] = "id</script>";
        book.config["valineAppKey"] = "key";
        book.config["clientId"] = "client";
        book.config["clientSecret"] = "secret";
        book.config["commentOwner"] = "owner";
        book.config["commentRepo"] = "repo";
        book.config["disqus"] = "test-site";
        const auto custom = temp.filePath("theme");
        if (theme == "custom") {
            require(StaticSite::copyDirectory("examples/site-theme", custom, &error), error);
            book.config["themeDirectory"] = custom;
        }
        require(StaticSite::prepare(book, &error), error);
        for (int i = 0; i < 12; ++i) {
            QJsonObject metadata{{"title", QString("Post <%1 &>").arg(i)}, {"slug", "post-" + QString::number(i)}, {"date", QString("2026-10-%1").arg(12 - i, 2, 10, QChar('0'))}, {"updated", "2026-10-24"}, {"categories", QJsonArray{"分类"}}, {"tags", QJsonArray{"tag"}}, {"keywords", "post，keyword"}, {"comments", i != 11}};
            const auto body = "# Heading\n\n## Child\n\nBody & text [other](post-1.md#heading) [asset](asset.txt)";
            write(book.path + "/source/_posts/post-" + QString::number(i) + ".md", "---\n" + QJsonDocument(metadata).toJson() + "---\n" + body);
        }
        write(book.path + "/source/_posts/asset.txt", "static asset");
        write(book.path + "/source/_posts/draft.md", "---\n{\"draft\":true}\n---\nHidden draft");
        write(book.path + "/source/_posts/unpublished.md", "---\npublished: false\n---\nUnpublished");
        write(book.path + "/source/_drafts/ignored.md", "Hidden source");
        fixedDates(book.path + "/source");
        require(StaticSite::generate(book, &error), theme + ": " + error);
        const auto output = book.path + "/public";
        for (const auto &route: {QString("index.html"), QString("page/2/index.html"), QString("post-0/index.html"), QString("categories/index.html"), QString("categories/分类/index.html"), QString("categories/分类/page/2/index.html"), QString("tags/tag/index.html"), QString("archives/2026/10/index.html"), QString("links/index.html")}) {
            const auto html = read(output + '/' + route);
            require(html.contains("/blog/") && html.contains("test-head") && html.contains("test-body") && html.contains("test-analytics"), theme + " page/root/injection: " + route);
        }
        const auto post = read(output + "/post-0/index.html");
        require(post.contains("Post &lt;0 &amp;&gt;") && post.contains("/blog/post-1/#heading") && post.contains("/blog/assets/posts/asset.txt"), theme + " escaping and rewritten source links");
        require(post.contains(platform == QString("gitalk") ? "new Gitalk" : platform == QString("disqus") ? "disqus_thread"
                                                                                                           : "new Valine"),
                theme + " comments");
        require(!read(output + "/post-11/index.html").contains("new Valine") && !read(output + "/post-11/index.html").contains("new Gitalk") && !read(output + "/post-11/index.html").contains("disqus_thread"), theme + " disabled comments");
        require(!read(output + "/links/index.html").contains("Bad friend"), "unsafe friend URL excluded");
        const auto search = QJsonDocument::fromJson(read(output + "/search.json")).array();
        require(search.size() == 12 && search.first().toObject()["url"] == "/blog/post-0/", "search excludes drafts and sorts posts");
        require(read(output + "/assets/posts/asset.txt") == "static asset", "source asset copied");
        snapshots[theme] = manifest(output);
        const auto preserved = snapshots[theme].toObject();
        const auto failsWithoutReplacement = [&] {
            error.clear();
            require(!StaticSite::generate(book, &error) && !error.isEmpty(), "invalid input must report failure");
            require(manifest(output) == preserved, "generation failure preserves every previous output file");
        };
        write(book.path + "/source/search.json", "conflicting output");
        failsWithoutReplacement();
        require(error.contains("冲突"), "output conflict error");
        QFile::remove(book.path + "/source/search.json");
        write(book.path + "/source/_posts/invalid.md", "---\nslug: ../outside\n---\ninvalid");
        failsWithoutReplacement();
        require(error.contains("路径"), "invalid route error");
        QFile::remove(book.path + "/source/_posts/invalid.md");
        if (theme == "custom") {
            const auto path = custom + "/layout/index.html";
            const auto original = read(path);
            for (const auto &invalid: {QByteArray(), QByteArray("<% body %>"), QByteArray("{% body %}"), QByteArray("{{unknown}}"), QByteArray("{{#author}}{{#title}}nested{{/title}}{{/author}}"), QByteArray("{{#title}}missing"), QByteArray("{{#title}}mismatch{{/author}}")}) {
                write(path, invalid);
                failsWithoutReplacement();
            }
            QFile::remove(path);
            failsWithoutReplacement();
            write(path, original);
            write(custom + "/source/uncompiled.scss", "$color: red;");
            failsWithoutReplacement();
            require(error.contains("预编译"), "custom assets require precompilation");
        }
    }
    const QString baseline = "tests/site-generation-baseline.json";
    if (qEnvironmentVariableIsSet("QSNOTE_RECORD_SITE_BASELINE")) write(baseline, QJsonDocument(snapshots).toJson());
    else {
        const auto expected = QJsonDocument::fromJson(read(baseline)).object();
        for (auto theme = expected.begin(); theme != expected.end(); ++theme) {
            const auto actual = snapshots[theme.key()].toObject();
            const auto pages = theme.value().toObject();
            require(actual.keys() == pages.keys(), theme.key() + " output paths unchanged");
            for (auto page = pages.begin(); page != pages.end(); ++page) require(actual[page.key()] == page.value(), theme.key() + " output bytes changed: " + page.key());
        }
    }
    qInfo() << "PASS: five themes, all routes/pagination/search/assets/comments/injection, invalid templates/paths/conflicts preserve output, byte-for-byte baseline";
}
