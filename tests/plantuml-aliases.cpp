#include "core/ArticleRenderer.h"
#include "core/EbookExporter.h"
#include "core/StaticSite.h"
#include "ui/MarkdownEditor.h"
#include <QWebEnginePage>
#include <QtWidgets>
#define private public
#include "ui/InplacePreview.h"
#include "ui/MarkdownPreview.h"
#undef private

namespace {
void require(bool condition, const QString &message) {
    if (!condition) qFatal("%s", qPrintable(message));
}
bool until(const std::function<bool()> &condition) {
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < 30000) {
        QEventLoop loop;
        QTimer::singleShot(20, &loop, &QEventLoop::quit);
        loop.exec();
    }
    return condition();
}
QVariant evaluate(QWebEnginePage *page, const QString &script) {
    QVariant result;
    bool done = false;
    page->runJavaScript(script, [&](const QVariant &value) { result = value; done = true; });
    require(until([&] { return done; }), "JavaScript callback timeout");
    return result;
}
QByteArray read(const QString &path) {
    QFile file(path);
    require(file.open(QIODevice::ReadOnly), path + ": " + file.errorString());
    return file.readAll();
}
}// namespace

int main(int argc, char **argv) {
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QApplication app(argc, argv);
    QTemporaryDir temp;
    require(temp.isValid(), "temporary directory");
    qputenv("XDG_CACHE_HOME", temp.path().toUtf8());
    qputenv("XDG_DATA_HOME", temp.path().toUtf8());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temp.path());
    QString error;
    const QString source = "@startuml\nAlice -> Bob: hello\n@enduml\n";
    ArticleRenderer renderer(temp.path(), "alias-regression");
    MarkdownPreview preview;
    preview.resize(800, 600);
    preview.show();
    MarkdownEditor editor;
    editor.resize(800, 600);
    editor.show();
    auto *inplace = editor.findChild<InplacePreview *>();
    require(inplace, "inplace renderer");
    for (const auto &language: {QString("puml"), QString("plantuml"), QString("PUML")}) {
        const auto markdown = "```" + language + "\n" + source + "```\n";
        QDomDocument document;
        require(renderer.render(markdown, language, document, &error), error);
        require(document.elementsByTagName("img").count() == 1, "export must render " + language);
        require(document.elementsByTagName("code").isEmpty(), "export must replace code fence");

        preview.setMarkdown(markdown, language);
        require(until([&] { return preview.renderedRevision_ == preview.revision(); }), "reading preview timeout");
        require(evaluate(preview.page(), "document.querySelectorAll('.diagram img').length").toInt() == 1, "reading preview " + language);
        editor.setText(markdown);
        require(until([&] { return inplace->requests_.size() == 1 && inplace->requests_.first().item; }), "inplace preview " + language);

        NotebookStore store;
        store.notebooks.append({language, "Aliases", "site", {{"post", {}, "Post", markdown, false, {{"slug", "post"}}}}, {}, temp.filePath(language)});
        require(store.save(&error), error);
        const auto &book = store.notebooks.first();
        require(StaticSite::generate(book, &error), error);
        QDirIterator pages(book.path + "/public", {"*.html"}, QDir::Files, QDirIterator::Subdirectories);
        bool siteImage = false;
        while (pages.hasNext()) siteImage |= read(pages.next()).contains("assets/qsnote/");
        require(siteImage, "site must reference rendered image");
        const auto images = QDir(book.path + "/public/assets/qsnote").entryList({"*.png"}, QDir::Files);
        require(!images.isEmpty() && !QImage(book.path + "/public/assets/qsnote/" + images.first()).isNull(), "site packaged image");
        auto ebook = book;
        ebook.type = "ebook";
        const auto html = EbookExporter::exportBook(ebook, {}, EbookExporter::Format::Html, temp.path(), &error);
        require(!html.isEmpty() && read(html).contains("data:image/png;base64,"), "HTML ebook: " + error);
        const auto epub = EbookExporter::exportBook(ebook, {}, EbookExporter::Format::Epub, temp.path(), &error);
        require(!epub.isEmpty() && QFileInfo(epub).size() > 0, "EPUB ebook: " + error);
        QProcess unzip;
        unzip.start("unzip", {"-l", epub});
        require(unzip.waitForFinished() && unzip.exitCode() == 0 && unzip.readAllStandardOutput().contains(".png"), "EPUB packaged image");
    }
    QDirIterator cache(QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/alias-regression", QDir::Files, QDirIterator::Subdirectories);
    while (cache.hasNext()) require(!cache.next().contains("/articles/"), "PlantUML aliases must bypass article cache");
    for (const auto &language: {QString("plantuml"), QString("puml")}) {
        const auto invalid = "```" + language + "\n@startuml\nthis is invalid !!!\n@enduml\n```\n";
        QDomDocument document;
        error.clear();
        require(!renderer.render(invalid, language, document, &error) && !error.isEmpty(), "invalid export must fail");
        preview.setMarkdown(invalid, language);
        require(until([&] { return preview.renderedRevision_ == preview.revision(); }), "invalid preview timeout");
        require(evaluate(preview.page(), "document.querySelectorAll('.render-error').length").toInt() == 1, "invalid preview error feedback");
    }
    QDomDocument other;
    require(renderer.render("```mermaid\ngraph TD; A-->B\n```\n```dot\ndigraph { A -> B }\n```\n```cpp\nint main() {}\n```\n", "other", other, &error), error);
    require(other.elementsByTagName("img").size() == 2 && !other.elementsByTagName("code").isEmpty(), "other diagrams and code unchanged");
    qInfo() << "PASS: plantuml/puml/PUML inplace, reading, site, HTML/EPUB, cache bypass, errors, Mermaid/Graphviz/code";
}
