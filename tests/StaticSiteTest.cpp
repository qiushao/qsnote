#include "core/StaticSite.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

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
QString siteCacheDirectory(const Notebook &site) {
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/sites/" +
        QString::fromLatin1(QCryptographicHash::hash(QDir(site.path).absolutePath().toUtf8(), QCryptographicHash::Sha256).toHex());
}
}

// NOLINTBEGIN(readability-convert-member-functions-to-static)
class StaticSiteTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        QStandardPaths::setTestModeEnabled(true);
    }
    void cachedArticlesTrackContentAndDependencies() {
        QTemporaryDir directory;
        Notebook site{"site", "缓存测试", "site", {}, {}, directory.path()};
        QString error;
        QVERIFY2(StaticSite::prepare(site, &error), qPrintable(error));
        const auto source = directory.path() + "/source/_posts/article.md";
        const auto linked = directory.path() + "/source/_posts/linked.md";
        const auto image = directory.path() + "/source/_posts/image.svg";
        const QByteArray body = "# Heading\n\nOLD [link](linked.md#target) ![image](image.svg)\n\n$x^2$\n\n```mermaid\ngraph LR\nA --> B\n```\n";
        QVERIFY(write(source, body));
        QVERIFY(write(linked, "---\nslug: before\n---\n# Target"));
        QVERIFY(write(image, "<svg xmlns=\"http://www.w3.org/2000/svg\"/>"));
        QVERIFY2(StaticSite::generate(site, &error), qPrintable(error));
        const auto output = directory.path() + "/public/";
        const auto first = read(output + "article/index.html");
        const auto assets = QDir(output + "assets/qsnote").entryList(QDir::Files);
        QCOMPARE(assets.size(), 2);
        QMap<QString, QByteArray> graphics;
        for (const auto &asset: assets) graphics[asset] = read(output + "assets/qsnote/" + asset);
        QVERIFY(first.contains("/before/#target"));
        QVERIFY(!QFile::exists(directory.path() + "/.qsnote-cache"));
        const auto cachePath = siteCacheDirectory(site) + "/articles/" +
            QString::fromLatin1(QCryptographicHash::hash("source/_posts/article.md", QCryptographicHash::Sha256).toHex());
        QFile cache(cachePath);
        QVERIFY(cache.open(QIODevice::ReadWrite));
        const auto timestamp = QDateTime::fromSecsSinceEpoch(1000000000);
        QVERIFY(cache.setFileTime(timestamp, QFileDevice::FileModificationTime));
        cache.close();
        QVERIFY2(StaticSite::generate(site, &error), qPrintable(error));
        QCOMPARE(QFileInfo(cachePath).lastModified(), timestamp); // Unchanged articles did not render again.
        QCOMPARE(read(output + "article/index.html"), first);
        for (auto it = graphics.cbegin(); it != graphics.cend(); ++it)
            QCOMPARE(read(output + "assets/qsnote/" + it.key()), it.value());

        site.config["domain"] = "https://example.com/blog/";
        site.config["theme"] = "next";
        QVERIFY(write(linked, "---\nslug: after\n---\n# Target"));
        const QByteArray updatedImage = R"(<svg xmlns="http://www.w3.org/2000/svg" width="42"/>)";
        QVERIFY(write(image, updatedImage));
        QVERIFY2(StaticSite::generate(site, &error), qPrintable(error));
        QCOMPARE(QFileInfo(cachePath).lastModified(), timestamp);
        const auto changed = read(output + "article/index.html");
        QVERIFY(changed.contains("/blog/after/#target"));
        QVERIFY(changed.contains("/blog/assets/posts/image.svg"));
        QVERIFY(changed.contains("/blog/assets/qsnote/"));
        QVERIFY(changed.contains("/blog/css/main.css"));
        QVERIFY(!changed.contains("/before/#target"));
        QCOMPARE(read(output + "assets/posts/image.svg"), updatedImage);
        QVERIFY(!QFile::exists(output + "before/index.html"));
        QVERIFY(!QFile::exists(output + ".qsnote-cache"));

        // A same-size edit with unchanged mtime must still invalidate the render.
        const auto sourceTime = QFileInfo(source).lastModified();
        QVERIFY(write(source, QByteArray(body).replace("OLD", "NEW")));
        QFile article(source);
        QVERIFY(article.open(QIODevice::ReadWrite));
        QVERIFY(article.setFileTime(sourceTime, QFileDevice::FileModificationTime));
        article.close();
        QVERIFY2(StaticSite::generate(site, &error), qPrintable(error));
        QVERIFY(read(output + "article/index.html").contains("NEW"));
        QVERIFY(!read(output + "article/index.html").contains("OLD"));
        QVERIFY(read(output + "search.json").contains("NEW"));
        QVERIFY(QFile::remove(source));
        QVERIFY2(StaticSite::generate(site, &error), qPrintable(error));
        QVERIFY(!QFile::exists(output + "article/index.html"));
        QVERIFY(!read(output + "search.json").contains("NEW"));
    }
    void unusableArticleCache_data() { // NOLINT(readability-identifier-naming)
        QTest::addColumn<QString>("failure");
        for (const auto &failure: {"missing", "old-version", "truncated", "unwritable"})
            QTest::newRow(failure) << QString(failure);
    }
    void unusableArticleCache() {
        QFETCH(QString, failure);
        QTemporaryDir directory;
        Notebook site{"site", "缓存测试", "site", {}, {}, directory.path()};
        QString error;
        QVERIFY2(StaticSite::prepare(site, &error), qPrintable(error));
        QVERIFY(write(directory.path() + "/source/_posts/article.md", "# Content\n\n**正文**"));
        QVERIFY2(StaticSite::generate(site, &error), qPrintable(error));
        const auto output = directory.path() + "/public/article/index.html";
        const auto previous = read(output);
        const auto cacheDirectory = siteCacheDirectory(site);
        const auto cachePath = cacheDirectory + "/articles/" +
            QString::fromLatin1(QCryptographicHash::hash("source/_posts/article.md", QCryptographicHash::Sha256).toHex());
        if (failure == "missing") QVERIFY(QFile::remove(cachePath));
        else if (failure == "old-version") QVERIFY(write(cachePath, "obsolete\n<article>STALE</article>"));
        else if (failure == "truncated") QVERIFY(write(cachePath, read(cachePath).left(70)));
        else {
            QVERIFY(QDir(cacheDirectory).removeRecursively());
            QVERIFY(write(cacheDirectory, "not a directory"));
        }
        QVERIFY2(StaticSite::generate(site, &error), qPrintable(error));
        QCOMPARE(read(output), previous);
    }
    void articleCachesAreIsolatedBySitePath() {
        QTemporaryDir first, second;
        Notebook site{"same-id", "站点", "site", {}, {}, first.path()};
        QString error;
        QVERIFY2(StaticSite::prepare(site, &error), qPrintable(error));
        QVERIFY(write(site.path + "/source/_posts/article.md", "First site"));
        QVERIFY2(StaticSite::generate(site, &error), qPrintable(error));
        const auto firstCache = siteCacheDirectory(site);
        const auto filename = "/articles/" + QString::fromLatin1(
            QCryptographicHash::hash("source/_posts/article.md", QCryptographicHash::Sha256).toHex());
        const auto firstRender = read(firstCache + filename);
        QVERIFY(!firstRender.isEmpty());

        site.path = second.path();
        QVERIFY2(StaticSite::prepare(site, &error), qPrintable(error));
        QVERIFY(write(site.path + "/source/_posts/article.md", "Second site"));
        QVERIFY2(StaticSite::generate(site, &error), qPrintable(error));
        QVERIFY(siteCacheDirectory(site) != firstCache);
        QVERIFY(read(siteCacheDirectory(site) + filename).contains("Second site"));
        QCOMPARE(read(firstCache + filename), firstRender);
        QVERIFY(!QFile::exists(first.path() + "/.qsnote-cache"));
        QVERIFY(!QFile::exists(second.path() + "/.qsnote-cache"));
    }
    void customTheme_data() { // NOLINT(readability-identifier-naming)
        QTest::addColumn<bool>("relative");
        QTest::newRow("relative") << true;
        QTest::newRow("absolute") << false;
    }
    void customTheme() {
        QFETCH(bool, relative);
        QTemporaryDir directory;
        const auto themePath = directory.path() + "/themes/自定义";
        QString error;
        QVERIFY2(StaticSite::copyDirectory(QFINDTESTDATA("../examples/site-theme"), themePath, &error), qPrintable(error));
        QVERIFY(write(themePath + "/layout/layout.html", read(themePath + "/layout/layout.html")
            + "<script>if (true) { if (true) { window.themeReady = true; }}</script>"));
        NotebookStore store;
        store.notebooks.append({"site", "自定义站点", "site", {},
            {{"theme", "custom"}, {"themeDirectory", relative ? QString("themes/自定义") : themePath},
             {"domain", "https://example.com/blog/"}, {"author", "<作者>{{body}}"}, {"token", "PRIVATE-TOKEN"},
             {"links", QJsonArray{QJsonObject{{"name", "友链"}, {"url", "https://example.org"}}}}}, directory.path()});
        auto &site = store.notebooks.first();
        for (int i = 0; i < 11; ++i)
            site.notes.append({QString::number(i), {}, "Post " + QString::number(i), "# Heading\n\n**Content**",
                false, {{"slug", "post-" + QString::number(i)}, {"date", "2026-10-20"}, {"category", "分类"}, {"tags", "标签"}}});
        const auto path = qgetenv("PATH");
        const auto restorePath = qScopeGuard([path] { qputenv("PATH", path); });
        qputenv("PATH", QByteArray());
        QVERIFY2(store.save(&error), qPrintable(error));
        Notebook restored;
        QVERIFY2(NotebookStore::readNotebook(directory.path(), restored, &error), qPrintable(error));
        QCOMPARE(restored.config["theme"].toString(), QString("custom"));
        QCOMPARE(restored.config["themeDirectory"], site.config["themeDirectory"]);
        QVERIFY(write(themePath + "/source/_ignored.txt", "ignored"));
        QVERIFY(write(themePath + "/source/.hidden", "hidden"));
        QVERIFY(write(themePath + "/_config.yml", "secret: NOT-PUBLISHED"));
        QVERIFY(write(themePath + "/scripts/test.js", "throw new Error('must not execute');"));
        QVERIFY2(StaticSite::generate(restored, &error), qPrintable(error));
        const auto output = directory.path() + "/public/";
        const auto index = read(output + "index.html");
        QVERIFY(index.contains("/blog/css/theme.css"));
        QVERIFY(index.contains("/blog/page/2/"));
        QVERIFY(index.contains("&lt;作者&gt;{{body}}"));
        QVERIFY(!index.contains("PRIVATE-TOKEN"));
        QVERIFY(!index.contains("NOT-PUBLISHED"));
        QVERIFY(!index.contains("class=\"main-center\""));
        QVERIFY(!QFile::exists(output + "_config.yml"));
        QVERIFY(!QFile::exists(output + "_ignored.txt"));
        QVERIFY(!QFile::exists(output + ".hidden"));
        QVERIFY(!QFile::exists(output + "scripts/test.js"));
        QCOMPARE(read(output + "css/theme.css"), read(themePath + "/source/css/theme.css"));
        QVERIFY(!read(output + "css/qsnote-article.css").isEmpty());
        for (const auto &route: {"post-0/", "page/2/", "archives/", "archives/2026/", "archives/2026/10/", "categories/", "categories/分类/", "tags/", "tags/标签/", "about/", "links/"})
            QVERIFY2(read(output + route + "index.html").contains("css/theme.css"), route);
        const auto post = read(output + "post-0/index.html");
        QVERIFY(post.contains("<strong>Content</strong>"));
        QVERIFY(post.contains("class=\"markdown-body\""));
        QVERIFY(post.contains("class=\"post-navigation\""));
        QVERIFY(post.contains("Heading</span>"));
        QVERIFY(read(output + "links/index.html").contains("https://example.org"));
        QVERIFY(write(themePath + "/layout/archive.html", "ARCHIVE {{body}} {{pagination}}"));
        QVERIFY(write(themePath + "/layout/tag.html", "TAG {{body}} {{pagination}}"));
        QVERIFY2(StaticSite::generate(restored, &error), qPrintable(error));
        QVERIFY(read(output + "archives/index.html").contains("ARCHIVE"));
        QVERIFY(read(output + "categories/分类/index.html").contains("ARCHIVE"));
        QVERIFY(read(output + "tags/标签/index.html").contains("TAG"));
        restored.config["theme"] = "pure";
        QVERIFY2(StaticSite::generate(restored, &error), qPrintable(error));
        QVERIFY(!QFile::exists(output + "css/theme.css"));
        QVERIFY(read(output + "index.html").contains("class=\"main-center\""));
    }
    void customThemeFailure_data() { // NOLINT(readability-identifier-naming)
        QTest::addColumn<QString>("failure");
        for (const auto &failure: {"empty-path", "missing-path", "missing-template", "empty-template", "ejs", "nunjucks", "unknown-variable", "nested-block", "unclosed-block", "styles", "collision"})
            QTest::newRow(failure) << QString(failure);
    }
    void customThemeFailure() {
        QFETCH(QString, failure);
        QTemporaryDir directory;
        Notebook site{"site", "站点", "site", {}, {{"theme", "custom"}, {"themeDirectory", "themes/test"}}, directory.path()};
        QString error;
        const auto themePath = directory.path() + "/themes/test";
        QVERIFY2(StaticSite::prepare(site, &error), qPrintable(error));
        QVERIFY2(StaticSite::copyDirectory(QFINDTESTDATA("../examples/site-theme"), themePath, &error), qPrintable(error));
        QVERIFY(write(directory.path() + "/public/index.html", "PREVIOUS"));
        if (failure == "empty-path") site.config["themeDirectory"] = "";
        else if (failure == "missing-path") site.config["themeDirectory"] = "absent";
        else if (failure == "missing-template") QVERIFY(QFile::remove(themePath + "/layout/post.html"));
        else if (failure == "empty-template") QVERIFY(write(themePath + "/layout/index.html", " "));
        else if (failure == "ejs") QVERIFY(write(themePath + "/layout/index.html", "<%= page.title %>"));
        else if (failure == "nunjucks") QVERIFY(write(themePath + "/layout/index.html", "{{ page.title }}"));
        else if (failure == "unknown-variable") QVERIFY(write(themePath + "/layout/index.html", "{{typo}}"));
        else if (failure == "nested-block") QVERIFY(write(themePath + "/layout/index.html", "{{#body}}{{#title}}{{title}}{{/title}}{{/body}}"));
        else if (failure == "unclosed-block") QVERIFY(write(themePath + "/layout/index.html", "{{#body}}{{body}}"));
        else if (failure == "styles") QVERIFY(write(themePath + "/source/css/theme.styl", "body\n  color red"));
        else if (failure == "collision") QVERIFY(write(themePath + "/source/index.html", "collision"));
        QVERIFY(!StaticSite::generate(site, &error));
        QVERIFY2(!error.isEmpty(), qPrintable(failure));
        QCOMPARE(read(directory.path() + "/public/index.html"), QByteArray("PREVIOUS"));
        QVERIFY(QDir(directory.path()).entryList({".qsnote-site-*"}, QDir::Dirs | QDir::Hidden).isEmpty());
    }
    void generateWithoutExternalPrograms_data() { // NOLINT(readability-identifier-naming): Qt Test data-provider convention.
        QTest::addColumn<QString>("theme");
        QTest::newRow("pure") << QString("pure");
        QTest::newRow("next") << QString("next");
        QTest::newRow("yilia") << QString("yilia");
        QTest::newRow("3-jekyll") << QString("3-jekyll");
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
                                "links/index.html", "about/index.html", "search.json"})
            QVERIFY2(QFile::exists(directory.path() + "/public/" + file), file);
        QVERIFY(QFile::exists(directory.path() + "/public/" + (theme == "3-jekyll" ? "theme-THIRD-PARTY-NOTICES.md" : "theme-LICENSE")));
        const auto article = read(directory.path() + "/public/article/index.html");
        for (const auto &content: {"<strong>加粗</strong>", "<table>", "language-cpp", "自定义页脚", "作者", "正文标题", "id=\"正文标题\"", "2026-10-09"})
            QVERIFY2(article.contains(content), content);
        QVERIFY(article.contains(theme == "pure" ? "class=\"main-center\"" : theme == "yilia" ? "class=\"article article-type-post\"" : theme == "3-jekyll" ? "class=\"post container\"" : "class=\"main-inner post posts-expand\""));
        QVERIFY(article.contains(theme == "pure" ? "/blog/css/style.css" : "/blog/css/main.css"));
        // Fingerprints of the original themes' compiled CSS: preserve layout, colors and breakpoints.
        const auto css = read(directory.path() + "/public/css/" + (theme == "pure" ? "style.css" : "main.css"));
        QCOMPARE(QCryptographicHash::hash(css, QCryptographicHash::Sha256).toHex(), theme == "pure"
            ? QByteArray("2aab838134abc02a55c42e3afcee6e867155aca4b0c28c7d66d1e1f74d0b9c10")
            : theme == "3-jekyll" ? QByteArray("2ed7ba6e517eea794bb4e71edd77e3b891c4ba438b9e61e30630e9229f5b0db9")
            : theme == "yilia" ? QByteArray("b0d7ccb28e00e916f5b7ea2c3e6c3fa3e204fef3a447f4de8415f8d7c9d869cd")
            : QByteArray("d5d3280e2cb167a84fca9c4459d6a5db0603b2512f201ce85bd79b6148d9d71e"));
        if (theme == "3-jekyll") {
            for (const auto &asset: {"js/qsnote-3-jekyll.js", "css/qsnote-3-jekyll.css", "assets/icon/icomoon.woff", "assets/img/avatar.jpg"})
                QVERIFY2(!read(directory.path() + "/public/" + asset).isEmpty(), asset);
            for (const auto &markup: {"id=\"sidebar-left\"", "id=\"sidebar-right\"", "id=\"main\"", "data-filter=\"标签一\"", "aria-current=\"page\"", "class=\"post-meta-span date\""})
                QVERIFY2(article.contains(markup), markup);
            QVERIFY(!article.contains("{{"));
            QVERIFY(!article.contains("fonts.googleapis.com"));
            QVERIFY(read(directory.path() + "/public/archives/index.html").contains("class=\"post-summary\""));
        }
        if (theme == "yilia") {
            for (const auto &asset: {"js/qsnote-yilia.js", "css/qsnote-yilia.css", "css/fonts/iconfont.8c627f.woff", "theme-THIRD-PARTY-NOTICES.md"})
                QVERIFY2(!read(directory.path() + "/public/" + asset).isEmpty(), asset);
            QVERIFY(article.contains("class=\"toc-article\""));
            QVERIFY(article.contains("class=\"left-col\""));
            QVERIFY(article.contains("id=\"mobile-nav\""));
            QVERIFY(!article.contains("{{"));
            const auto content = QJsonDocument::fromJson(read(directory.path() + "/public/content.json")).object()["posts"].toArray();
            QCOMPARE(content.size(), 1);
            QCOMPARE(content.first().toObject()["path"].toString(), QString("article/"));
            QCOMPARE(content.first().toObject()["tags"].toArray().size(), 2);
            QVERIFY(read(directory.path() + "/public/archives/index.html").contains("class=\"archives-wrap\""));
        }
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
        if (theme == "yilia") {
            QVERIFY(!QFile::exists(directory.path() + "/public/js/qsnote-yilia.js"));
            QVERIFY(!QFile::exists(directory.path() + "/public/css/qsnote-yilia.css"));
        }
        if (theme == "3-jekyll") {
            QVERIFY(!QFile::exists(directory.path() + "/public/js/qsnote-3-jekyll.js"));
            QVERIFY(!QFile::exists(directory.path() + "/public/assets/icon/icomoon.woff"));
        }
        QVERIFY(read(directory.path() + "/public/index.html").contains(theme == "pure" ? "class=\"main-inner index posts-expand\"" : "class=\"main-center\""));
        QTemporaryDir exported;
        QVERIFY2(StaticSite::copyDirectory(directory.path() + "/public", exported.path(), &error), qPrintable(error));
        QCOMPARE(read(exported.path() + "/article/index.html"), read(directory.path() + "/public/article/index.html"));
        const auto articleCache = siteCacheDirectory(site) + "/articles/" +
            QString::fromLatin1(QCryptographicHash::hash(site.notes.first().path.toUtf8(), QCryptographicHash::Sha256).toHex());
        const auto aboutCache = siteCacheDirectory(site) + "/articles/" +
            QString::fromLatin1(QCryptographicHash::hash("source/about/index.md", QCryptographicHash::Sha256).toHex());
        QVERIFY(QFile::exists(articleCache));
        const auto aboutRender = read(aboutCache);
        QVERIFY(!aboutRender.isEmpty());
        NotebookStore::removeNoteFiles(site, site.notes.first());
        QVERIFY(!QFile::exists(articleCache));
        QCOMPARE(read(aboutCache), aboutRender);
        site.notes.removeFirst();
        QVERIFY2(store.save(&error), qPrintable(error));
        QVERIFY2(StaticSite::generate(site, &error), qPrintable(error));
        QVERIFY(!QFile::exists(directory.path() + "/public/article/index.html"));
        QVERIFY(!QFile::exists(directory.path() + "/public/categories/分类/index.html"));
        QCOMPARE(QJsonDocument::fromJson(read(directory.path() + "/public/search.json")).array().size(), 0);
    }

    void profileAndKeywords_data() { generateWithoutExternalPrograms_data(); } // NOLINT(readability-identifier-naming)
    void profileAndKeywords() {
        QFETCH(QString, theme);
        QTemporaryDir directory;
        NotebookStore store;
        store.notebooks.append({"site", "站点", "site", {},
            {{"theme", theme}, {"github", "https://github.com/example?tab=repositories&language=c++"},
             {"city", "杭州 <西湖>"}, {"keywords", "站点，C++ & Qt"}}, directory.path()});
        auto &site = store.notebooks.first();
        site.notes.append({"post", {}, "文章", "正文", false,
            {{"slug", "article"}, {"keywords", "文章, \"关键词\""}}});
        QString error;
        QVERIFY2(store.save(&error), qPrintable(error));
        Notebook restored;
        QVERIFY2(NotebookStore::readNotebook(site.path, restored, &error), qPrintable(error));
        for (const auto &key: {"github", "city", "keywords"}) QCOMPARE(restored.config[key], site.config[key]);
        QCOMPARE(restored.notes.first().metadata["keywords"], site.notes.first().metadata["keywords"]);
        QVERIFY(read(site.path + "/" + site.notes.first().path).contains("\"keywords\""));
        QVERIFY2(StaticSite::generate(restored, &error), qPrintable(error));
        const auto siteMeta = QByteArray(R"(<meta name="keywords" content="站点,C++ &amp; Qt">)");
        for (const auto &route: {"index.html", "archives/index.html", "about/index.html"}) {
            const auto html = read(site.path + "/public/" + route);
            QVERIFY(html.contains(siteMeta));
            QCOMPARE(html.contains("杭州 &lt;西湖&gt;"), theme == "pure");
            QVERIFY(html.contains("href=\"https://github.com/example?tab=repositories&amp;language=c++\""));
            QVERIFY(!html.contains("{{"));
            QVERIFY(!html.contains("class=\"site-city\""));
            QVERIFY(!html.contains("class=\"site-github\""));
            const auto socialClass = theme == "pure" ? "icon icon-github" : theme == "next" ? "fab fa-github fa-fw"
                : theme == "yilia" ? "icon-github" : "sidebar-social-icon github";
            QVERIFY(html.contains(socialClass));
            if (theme == "pure")
                QVERIFY(html.contains(R"(<small class="text-muted hidden-xs hidden-sm" id="location"><i class="icon icon-map-marker"></i> 杭州 &lt;西湖&gt;</small>)"));
        }
        const auto article = read(site.path + "/public/article/index.html");
        QVERIFY(article.contains(R"(<meta name="keywords" content="文章, &quot;关键词&quot;">)"));
        QVERIFY(!article.contains(siteMeta));

        // Clearing the article keywords restores the site default.
        site.notes.first().metadata["keywords"] = "";
        QVERIFY2(store.save(&error), qPrintable(error));
        QVERIFY2(StaticSite::generate(site, &error), qPrintable(error));
        QVERIFY(read(site.path + "/public/article/index.html").contains(siteMeta));

        // Author-written front matter accepts keyword lists too.
        QVERIFY(write(site.path + "/source/_posts/manual.md", "---\nkeywords: [manual, seo]\n---\n正文"));
        site.config["keywords"] = "";
        site.config["city"] = "";
        site.config["github"] = "javascript:alert(1)";
        QVERIFY2(StaticSite::generate(site, &error), qPrintable(error));
        QVERIFY(read(site.path + "/public/manual/index.html").contains(R"(<meta name="keywords" content="manual, seo">)"));
        const auto empty = read(site.path + "/public/article/index.html");
        QVERIFY(!empty.contains("name=\"keywords\""));
        QVERIFY(!empty.contains("class=\"site-city\""));
        QVERIFY(!empty.contains("class=\"site-github\""));
        QVERIFY(!empty.contains("aria-label=\"GitHub\""));
        QVERIFY(!empty.contains("{{"));
        site.config["github"] = "";
        QVERIFY2(StaticSite::generate(site, &error), qPrintable(error));
        const auto unset = read(site.path + "/public/index.html");
        QVERIFY(!unset.contains("aria-label=\"GitHub\""));
        QVERIFY(!unset.contains("{{"));
    }

    void customAvatar_data() { // NOLINT(readability-identifier-naming): Qt Test data-provider convention.
        generateWithoutExternalPrograms_data();
    }
    void customAvatar() {
        QFETCH(QString, theme);
        QTemporaryDir directory;
        Notebook site{"site", "站点", "site", {}, {{"theme", theme}, {"domain", "https://example.com/blog/"}}, directory.path()};
        QString error;
        QVERIFY2(StaticSite::prepare(site, &error), qPrintable(error));
        QVERIFY(write(directory.path() + "/source/_posts/post.md", "---\nslug: nested/article\n---\n正文"));
        const auto avatarPath = QStringLiteral("images/我的头像.png");
        const auto image = read(":/site/pure/assets/images/avatar.jpg");
        QVERIFY(!image.isEmpty());
        QVERIFY(write(directory.path() + "/source/" + avatarPath, image));
        site.config["avatar"] = avatarPath;
        QVERIFY2(StaticSite::generate(site, &error), qPrintable(error));
        const auto expected = "src=\"/blog/" + QUrl::toPercentEncoding(avatarPath, "/") + "\"";
        for (const auto &route: {"index.html", "nested/article/index.html"})
            QVERIFY(read(directory.path() + "/public/" + route).contains(expected));
        QCOMPARE(read(directory.path() + "/public/" + avatarPath), image);
        if (theme == "yilia") QCOMPARE(read(directory.path() + "/public/index.html").count(expected), 2);
        site.config["avatar"] = "https://example.org/avatar.png?size=128&name=\"author\"";
        QVERIFY2(StaticSite::generate(site, &error), qPrintable(error));
        QVERIFY(read(directory.path() + "/public/index.html").contains("src=\"https://example.org/avatar.png?size=128&amp;name=&quot;author&quot;\""));
        site.config["avatar"] = "";
        QVERIFY2(StaticSite::generate(site, &error), qPrintable(error));
        const auto html = read(directory.path() + "/public/index.html");
        QVERIFY(!html.contains(expected));
        if (theme == "yilia") QVERIFY(!html.contains("<img class=\"show\""));
        else QVERIFY(html.contains(theme == "next" ? "src=\"/blog/images/avatar.gif\""
                                   : theme == "3-jekyll" ? "src=\"/blog/assets/img/avatar.jpg\"" : "src=\"/blog/images/avatar.jpg\""));
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
