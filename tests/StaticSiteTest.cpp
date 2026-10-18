#include "core/StaticSite.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

namespace {
QByteArray read(const QString &path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}
bool write(const QString &path, const QByteArray &data) {
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) return false;
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
}
}

// NOLINTBEGIN(readability-convert-member-functions-to-static)
class StaticSiteTest : public QObject {
    Q_OBJECT
private slots:
    void generateWithoutExternalPrograms_data() { // NOLINT(readability-identifier-naming): Qt Test data-provider convention.
        QTest::addColumn<QString>("theme");
        QTest::newRow("pure") << QString("pure");
        QTest::newRow("next") << QString("next");
    }
    void generateWithoutExternalPrograms() {
        QFETCH(QString, theme);
        QStandardPaths::setTestModeEnabled(true);
        QTemporaryDir directory;
        NotebookStore store;
        store.notebooks.append({"site", "站点", "site", {},
            {{"theme", theme}, {"title", "原生站点"}, {"description", "站点描述"}, {"author", "作者"},
             {"domain", "https://example.com/blog/"}, {"footer", "自定义页脚"}, {"token", "PRIVATE-DEPLOY-TOKEN"},
             {"links", QJsonArray{QJsonObject{{"name", "示例友链"}, {"url", "https://example.org"}}}}}, directory.path()});
        auto &site = store.notebooks.first();
        site.notes.append({"post", {}, "文章标题", "# 正文标题\n\n真实文章 **加粗**\n\n| A | B |\n|---|---|\n| 一 | 二 |\n\n~~删除~~\n\n```cpp\nint answer = 42;\n```", false,
                           {{"slug", "article"}, {"date", "2026-10-09"}, {"category", "分类"}, {"tags", "标签一, 标签二"}}});
        site.notes.append({"draft", {}, "草稿", "DRAFT-MUST-NOT-BE-PUBLISHED", false, {{"draft", true}}});
        QString error;
        const auto path = qgetenv("PATH");
        const auto restorePath = qScopeGuard([path] { qputenv("PATH", path); });
        qputenv("PATH", QByteArray());
        QVERIFY2(store.save(&error), qPrintable(error));
        QCOMPARE(site.notes.first().path, QString("source/_posts/分类/文章标题.md"));
        for (const auto &file: {"package.json", "package-lock.json", "node_modules", "qsnote-generate.cjs", "themes", "_config.yml"})
            QVERIFY2(!QFile::exists(directory.path() + "/" + file), file);
        Notebook restored;
        QVERIFY2(NotebookStore::readNotebook(directory.path(), restored, &error), qPrintable(error));
        QCOMPARE(restored.notes.first().content, site.notes.first().content);
        QCOMPARE(restored.config["theme"].toString(), theme);
        QVERIFY2(StaticSite::generate(site, &error), qPrintable(error));
        for (const auto &file: {"index.html", "article/index.html", "archives/index.html", "archives/2026/index.html", "archives/2026/10/index.html",
                                "categories/index.html", "categories/分类/index.html", "tags/index.html", "tags/标签一/index.html",
                                "links/index.html", "about/index.html", "theme-LICENSE", "search.json"})
            QVERIFY2(QFile::exists(directory.path() + "/public/" + file), file);
        const auto article = read(directory.path() + "/public/article/index.html");
        for (const auto &content: {"<strong>加粗</strong>", "<table>", "language-cpp", "自定义页脚", "作者", "正文标题", "id=\"正文标题\"", "2026-10-09"})
            QVERIFY2(article.contains(content), content);
        QVERIFY(article.contains(theme == "pure" ? "class=\"main-center\"" : "class=\"main-inner post posts-expand\""));
        QVERIFY(article.contains(theme == "pure" ? "/blog/css/style.css" : "/blog/css/main.css"));
        // Fingerprints of the original themes' compiled CSS: preserve layout, colors and breakpoints.
        const auto css = read(directory.path() + "/public/css/" + (theme == "pure" ? "style.css" : "main.css"));
        QCOMPARE(QCryptographicHash::hash(css, QCryptographicHash::Sha256).toHex(), theme == "pure"
            ? QByteArray("2aab838134abc02a55c42e3afcee6e867155aca4b0c28c7d66d1e1f74d0b9c10")
            : QByteArray("d5d3280e2cb167a84fca9c4459d6a5db0603b2512f201ce85bd79b6148d9d71e"));
        QVERIFY(!article.contains("PRIVATE-DEPLOY-TOKEN"));
        QVERIFY(!article.contains("new Valine"));
        QVERIFY(read(directory.path() + "/public/links/index.html").contains("https://example.org"));
        const auto search = QJsonDocument::fromJson(read(directory.path() + "/public/search.json")).array();
        QCOMPARE(search.size(), 1);
        QCOMPARE(search.first().toObject()["url"].toString(), QString("/blog/article/"));
        QVERIFY(!read(directory.path() + "/public/search.json").contains("DRAFT-MUST-NOT-BE-PUBLISHED"));
        QVERIFY(!QFile::exists(directory.path() + "/public/_drafts"));
        QVERIFY(!QFile::exists(directory.path() + "/public/_index.json"));
        // Metadata updates remain round-trippable without duplicating front matter.
        site.notes.first().metadata["tags"] = "修改标签";
        QVERIFY2(store.save(&error), qPrintable(error));
        QVERIFY2(store.save(&error), qPrintable(error));
        QCOMPARE(read(directory.path() + "/" + site.notes.first().path).count("\"qsnote\""), 1);
        QVERIFY(write(directory.path() + "/source/about/index.md", "---\n{\"title\":\"关于我们\"}\n---\nCustom about page"));
        site.config["theme"] = theme == "pure" ? "next" : "pure";
        QVERIFY2(store.save(&error), qPrintable(error));
        QVERIFY2(StaticSite::generate(site, &error), qPrintable(error));
        QVERIFY(read(directory.path() + "/public/about/index.html").contains("Custom about page"));
        QVERIFY(read(directory.path() + "/public/index.html").contains(theme == "pure" ? "class=\"main-inner index posts-expand\"" : "class=\"main-center\""));
        QTemporaryDir exported;
        QVERIFY2(StaticSite::copyDirectory(directory.path() + "/public", exported.path(), &error), qPrintable(error));
        QCOMPARE(read(exported.path() + "/article/index.html"), read(directory.path() + "/public/article/index.html"));
        NotebookStore::removeNoteFiles(site, site.notes.first());
        site.notes.removeFirst();
        QVERIFY2(store.save(&error), qPrintable(error));
        QVERIFY2(StaticSite::generate(site, &error), qPrintable(error));
        QVERIFY(!QFile::exists(directory.path() + "/public/article/index.html"));
        QVERIFY(!QFile::exists(directory.path() + "/public/categories/分类/index.html"));
        QCOMPARE(QJsonDocument::fromJson(read(directory.path() + "/public/search.json")).array().size(), 0);
    }

    void migrateRemovedTheme() {
        QTemporaryDir directory;
        NotebookStore store;
        store.notebooks.append({"site", "站点", "site", {}, {{"theme", "stellar"}}, directory.path()});
        auto &site = store.notebooks.first();
        QString error;
        QVERIFY2(store.save(&error), qPrintable(error));
        QCOMPARE(site.config["theme"].toString(), QString("next"));
        const auto index = directory.path() + "/" + NotebookStore::indexFileName();
        QVERIFY(write(index, read(index).replace("\"next\"", "\"stellar\"")));
        Notebook restored;
        QVERIFY2(NotebookStore::readNotebook(directory.path(), restored, &error), qPrintable(error));
        QCOMPARE(restored.config["theme"].toString(), QString("next"));
        QVERIFY(write(directory.path() + "/public/js/stellar-icons.js", "obsolete"));
        QVERIFY2(StaticSite::generate(restored, &error), qPrintable(error));
        QVERIFY(read(directory.path() + "/public/index.html").contains("NexT.Muse"));
        QVERIFY(!QFile::exists(directory.path() + "/public/js/stellar-icons.js"));
        QVERIFY(!QFile::exists(":/site/stellar/assets/css/main.css"));
    }

    void invalidDiagramPreservesPublishedOutput() {
        QTemporaryDir directory;
        Notebook site{"site", "网站", "site", {}, {}, directory.path()};
        QString error;
        QVERIFY2(StaticSite::prepare(site, &error), qPrintable(error));
        const auto source = directory.path() + "/source/_posts/article.md";
        QVERIFY(write(source, "原有正文"));
        QVERIFY2(StaticSite::generate(site, &error), qPrintable(error));
        const auto previous = read(directory.path() + "/public/article/index.html");
        QVERIFY(!previous.isEmpty());
        QVERIFY(write(source, "```mermaid\nnot a diagram\n```"));
        QVERIFY(!StaticSite::generate(site, &error));
        QVERIFY(error.contains("article"));
        QVERIFY(error.contains("Mermaid"));
        QCOMPARE(read(directory.path() + "/public/article/index.html"), previous);
    }

    void paginationLinksAndFrontMatter_data() { generateWithoutExternalPrograms_data(); } // NOLINT(readability-identifier-naming)
    void paginationLinksAndFrontMatter() {
        QFETCH(QString, theme);
        QTemporaryDir directory;
        Notebook site{"site", "测试", "site", {}, {{"theme", theme}, {"domain", "https://example.com/中文/"}}, directory.path()};
        QString error;
        QVERIFY2(StaticSite::prepare(site, &error), qPrintable(error));
        for (int i = 1; i <= 12; ++i) {
            const auto content = "---\ntitle: 文章 " + QByteArray::number(i) + "\nslug: post-" + QByteArray::number(i) +
                "\ndate: 2026-10-" + QByteArray::number(i).rightJustified(2, '0') + "\ncategories:\n  - 分类\ntags: [甲, '乙']\n---\n正文";
            QVERIFY(write(directory.path() + "/source/_posts/" + QString::number(i) + ".md", content));
        }
        QVERIFY(write(directory.path() + "/source/_posts/linked.md", "---\n{\"title\":\"链接\",\"slug\":\"linked\",\"date\":\"2026-10-13\"}\n---\n[文章](1.md#正文) ![图片](images/pic.svg) [页面](../guide/index.md) [外链](https://example.org/x) [根路径](/guide/)\n\n## 标题\n\n## 标题\n"));
        QVERIFY(write(directory.path() + "/source/_posts/images/pic.svg", "<svg xmlns=\"http://www.w3.org/2000/svg\"/>"));
        QVERIFY(write(directory.path() + "/source/guide/index.md", "---\ntitle: 指南\n---\n[下载](manual.txt)"));
        QVERIFY(write(directory.path() + "/source/guide/manual.txt", "manual"));
        QVERIFY(write(directory.path() + "/source/CNAME", "example.com"));
        QVERIFY(write(directory.path() + "/source/_posts/hidden.md", "---\npublished: false\n---\nHIDDEN-CONTENT"));
        QVERIFY2(StaticSite::generate(site, &error), qPrintable(error));
        for (const auto &route: {"page/2/index.html", "categories/分类/page/2/index.html", "tags/甲/page/2/index.html", "archives/2026/10/page/2/index.html", "guide/index.html"})
            QVERIFY2(QFile::exists(directory.path() + "/public/" + route), route);
        const auto home = read(directory.path() + "/public/index.html");
        QVERIFY(home.indexOf("post-12/") < home.indexOf("post-11/"));
        QVERIFY(home.contains("/%E4%B8%AD%E6%96%87/page/2/"));
        const auto linked = read(directory.path() + "/public/linked/index.html");
        for (const auto &url: {"/%E4%B8%AD%E6%96%87/post-1/#", "/%E4%B8%AD%E6%96%87/assets/posts/images/pic.svg", "/%E4%B8%AD%E6%96%87/guide/", "https://example.org/x", "id=\"标题-1\""})
            QVERIFY2(linked.contains(url), url);
        QCOMPARE(read(directory.path() + "/public/guide/manual.txt"), QByteArray("manual"));
        QCOMPARE(read(directory.path() + "/public/CNAME"), QByteArray("example.com"));
        QVERIFY(QFile::exists(directory.path() + "/public/assets/posts/images/pic.svg"));
        QVERIFY(!read(directory.path() + "/public/search.json").contains("HIDDEN-CONTENT"));
        QVERIFY(!QFile::exists(directory.path() + "/public/hidden"));
    }

    void commentsAndEscaping_data() { generateWithoutExternalPrograms_data(); } // NOLINT(readability-identifier-naming)
    void commentsAndEscaping() {
        QFETCH(QString, theme);
        QTemporaryDir directory;
        Notebook site{"site", "<script>title</script>", "site", {},
            {{"theme", theme}, {"footer", "<b>footer</b>"}, {"valineAppId", "</script><script>alert('x')</script>"},
             {"valineAppKey", "key"}, {"clientSecret", "secret"}, {"disqus", "example-comments"}}, directory.path()};
        QString error;
        QVERIFY2(StaticSite::prepare(site, &error), qPrintable(error));
        QVERIFY(write(directory.path() + "/source/_posts/post.md", "正文"));
        for (const auto &platform: {"valine", "gitalk", "disqus", "none"}) {
            site.config["commentPlatform"] = platform;
            QVERIFY2(StaticSite::generate(site, &error), qPrintable(error));
            const auto article = read(directory.path() + "/public/post/index.html");
            QVERIFY(article.contains("&lt;script&gt;title&lt;/script&gt;"));
            QVERIFY(article.contains("&lt;b&gt;footer&lt;/b&gt;"));
            QVERIFY(!article.contains("</script><script>alert('x')"));
            QCOMPARE(article.contains("new Valine("), QByteArray(platform) == "valine");
            QCOMPARE(article.contains("new Gitalk("), QByteArray(platform) == "gitalk");
            QCOMPARE(article.contains("example-comments.disqus.com/embed.js"), QByteArray(platform) == "disqus");
            QVERIFY(!read(directory.path() + "/public/index.html").contains("new Valine("));
        }
        site.config["commentPlatform"] = "valine";
        QVERIFY(write(directory.path() + "/source/_posts/post.md", "---\ncomments: false\n---\n正文"));
        QVERIFY2(StaticSite::generate(site, &error), qPrintable(error));
        QVERIFY(!read(directory.path() + "/public/post/index.html").contains("new Valine("));
    }

    void failurePreservesPreviousOutput_data() { // NOLINT(readability-identifier-naming): Qt Test data-provider convention.
        QTest::addColumn<QByteArray>("source");
        QTest::newRow("traversal") << QByteArray("---\nslug: ../escape\n---\n正文");
        QTest::newRow("encoded-traversal") << QByteArray("---\nslug: %2e%2e/escape\n---\n正文");
        QTest::newRow("absolute") << QByteArray("---\nslug: /escape\n---\n正文");
        QTest::newRow("duplicate") << QByteArray("---\nslug: first\n---\n正文");
        QTest::newRow("double-slash") << QByteArray("---\nslug: first//child\n---\n正文");
        QTest::newRow("reserved") << QByteArray("---\nslug: archives\n---\n正文");
        QTest::newRow("bad-json") << QByteArray("---\n{broken}\n---\n正文");
        QTest::newRow("bad-yaml") << QByteArray("---\ntitle: test\n  nested: no\n---\n正文");
        QTest::newRow("unclosed-header") << QByteArray("---\ntitle: test\n正文");
    }
    void failurePreservesPreviousOutput() {
        QFETCH(QByteArray, source);
        QTemporaryDir directory;
        Notebook site{"site", "站点", "site", {}, {}, directory.path()};
        QString error;
        QVERIFY2(StaticSite::prepare(site, &error), qPrintable(error));
        QVERIFY(write(directory.path() + "/source/_posts/first.md", "# 正文"));
        QVERIFY2(StaticSite::generate(site, &error), qPrintable(error));
        const auto previous = read(directory.path() + "/public/index.html");
        QVERIFY(write(directory.path() + "/source/_posts/invalid.md", source));
        QVERIFY(!StaticSite::generate(site, &error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(read(directory.path() + "/public/index.html"), previous);
        QVERIFY(!QFile::exists(directory.path() + "/escape"));
        QVERIFY(QDir(directory.path()).entryList({".qsnote-site-*"}, QDir::Dirs | QDir::Hidden).isEmpty());
    }
};
// NOLINTEND(readability-convert-member-functions-to-static)
QTEST_MAIN(StaticSiteTest)
#include "StaticSiteTest.moc"
