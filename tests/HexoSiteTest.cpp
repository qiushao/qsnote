#include "core/HexoSite.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

namespace {
QByteArray read(const QString &path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}
}

// NOLINTBEGIN(readability-convert-member-functions-to-static)
class HexoSiteTest : public QObject {
    Q_OBJECT
private slots:
    void createMigrateAndGenerate() {
        QStandardPaths::setTestModeEnabled(true);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        NotebookStore store;
        Notebook site{"site", "站点", "site", {}, {{"theme", "tech"}, {"title", "测试站点"}, {"author", "测试作者"},
                      {"domain", "https://example.com/blog/"}, {"footer", "测试页脚"}, {"token", "PRIVATE-DEPLOY-TOKEN"},
                      {"links", QJsonArray{QJsonObject{{"name", "示例友链"}, {"url", "https://example.org"}}}}}, directory.path()};
        Note post{"post", {}, "文章标题", "# 正文标题\n\n真实文章内容 **加粗**", false,
                  {{"date", "2026-10-09"}, {"slug", "article"}, {"category", "分类"}, {"tags", "标签一, 标签二"}}, "旧分类/文章.md"};
        QVERIFY(QDir().mkpath(directory.path() + "/旧分类"));
        QFile old(directory.path() + "/" + post.path);
        QVERIFY(old.open(QIODevice::WriteOnly));
        old.write(post.content.toUtf8());
        old.close();
        site.notes.append(post);
        site.notes.append({"draft", {}, "草稿", "DRAFT-MUST-NOT-BE-PUBLISHED", false, {{"draft", true}}});
        store.notebooks.append(site);
        QString error;
        QVERIFY2(store.save(&error), qPrintable(error));
        const auto &saved = store.notebooks.first();
        QCOMPARE(saved.config["theme"].toString(), QString("pure"));
        QCOMPARE(saved.notes.first().path, QString("source/_posts/分类/文章标题.md"));
        QVERIFY(!QFile::exists(old.fileName()));
        QCOMPARE(saved.notes.last().path, QString("source/_drafts/草稿.md"));
        for (const auto &file: {"_config.yml", "package.json", "package-lock.json", "scaffolds/post.md", "themes/pure/layout/index.ejs", "themes/pure/source/css/style.css", "themes/pure/LICENSE"})
            QVERIFY2(QFile::exists(directory.path() + "/" + file), file);
        const auto source = read(directory.path() + "/" + saved.notes.first().path);
        QVERIFY(source.startsWith("---\n"));
        QVERIFY(source.contains("\"slug\": \"article\""));
        QVERIFY(!read(directory.path() + "/_config.yml").contains("PRIVATE-DEPLOY-TOKEN"));
        Notebook restored;
        QVERIFY2(NotebookStore::readNotebook(directory.path(), restored, &error), qPrintable(error));
        QCOMPARE(restored.notes.first().content, post.content);
        // Metadata-only edits must reach front matter, and repeated saves must not duplicate it.
        store.notebooks.first().notes.first().metadata["tags"] = "修改标签";
        QVERIFY2(store.save(&error), qPrintable(error));
        QVERIFY2(store.save(&error), qPrintable(error));
        QCOMPARE(read(directory.path() + "/" + saved.notes.first().path).count("\"qsnote\""), 1);
        QVERIFY2(HexoSite::generate(saved, &error), qPrintable(error));
        for (const auto &file: {"index.html", "article/index.html", "archives/index.html", "categories/index.html", "tags/index.html", "links/index.html", "about/index.html", "css/style.css", "js/jquery.min.js", "js/insight.js", "content.json"})
            QVERIFY2(QFile::exists(directory.path() + "/public/" + file), file);
        const auto article = read(directory.path() + "/public/article/index.html");
        QVERIFY(article.contains("真实文章内容"));
        QVERIFY(article.contains("<strong>加粗</strong>"));
        QVERIFY(article.contains("测试页脚"));
        QVERIFY(article.contains("/blog/css/style.css"));
        QVERIFY(article.contains("/blog/js/jquery.min.js"));
        QVERIFY(article.contains("article-title"));
        QVERIFY(read(directory.path() + "/public/links/index.html").contains("https://example.org"));
        QVERIFY(read(directory.path() + "/public/content.json").contains("article"));
        QVERIFY(!QFile::exists(directory.path() + "/public/config.json"));
        QVERIFY(!QFile::exists(directory.path() + "/public/草稿/index.html"));
        QVERIFY(!read(directory.path() + "/public/content.json").contains("DRAFT-MUST-NOT-BE-PUBLISHED"));
        QTemporaryDir exported;
        QVERIFY2(HexoSite::copyDirectory(directory.path() + "/public", exported.path(), &error), qPrintable(error));
        QCOMPARE(read(exported.path() + "/article/index.html"), article);
        QCOMPARE(read(exported.path() + "/css/style.css"), read(directory.path() + "/public/css/style.css"));
        // A template error must fail generation rather than report a successful export.
        const auto templatePath = directory.path() + "/themes/pure/layout/post.ejs";
        const auto originalTemplate = read(templatePath);
        QFile brokenTemplate(templatePath);
        QVERIFY(brokenTemplate.open(QIODevice::WriteOnly));
        brokenTemplate.write("<% if ( %>");
        brokenTemplate.close();
        QVERIFY(!HexoSite::generate(saved, &error));
        QVERIFY(error.contains("post.ejs"));
        QVERIFY(brokenTemplate.open(QIODevice::WriteOnly));
        brokenTemplate.write(originalTemplate);
        brokenTemplate.close();
        // Missing Node.js returns an actionable error without changing the site.
        const auto path = qgetenv("PATH");
        qputenv("PATH", QByteArray());
        const auto withoutNode = HexoSite::generate(saved, &error);
        qputenv("PATH", path);
        QVERIFY(!withoutNode);
        QVERIFY(error.contains("Node.js"));
        // Deleting/renaming posts must not leave obsolete public routes on a subsequent build.
        NotebookStore::removeNoteFiles(saved, saved.notes.first());
        store.notebooks.first().notes.clear();
        QVERIFY2(store.save(&error), qPrintable(error));
        QVERIFY2(HexoSite::generate(saved, &error), qPrintable(error));
        QVERIFY(!QFile::exists(directory.path() + "/public/article/index.html"));
    }
};
// NOLINTEND(readability-convert-member-functions-to-static)
QTEST_GUILESS_MAIN(HexoSiteTest)
#include "HexoSiteTest.moc"
