#include "core/SiteTemplateRenderer.h"
#include "core/StaticSite.h"
#include <QtCore>

namespace {
void require(bool ok, const char *message) {
    if (!ok) qFatal("%s", message);
}
void write(const QString &path, const QByteArray &bytes) {
    QFile file(path);
    require(file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size(), "fixture write");
}
}// namespace
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir temp;
    QString error;
    require(StaticSite::copyDirectory("examples/site-theme", temp.filePath("theme"), &error), "theme fixture");
    Notebook book{"site", "Site", "site", {}, {{"theme", "custom"}, {"themeDirectory", "theme"}}, temp.path()};
    const auto path = temp.filePath("theme/layout/index.html");
    write(path, "before{{#author}}A:{{author}}{{/author}}|{{#city}}C:{{city}}{{/city}}|{{body}}|{{title}}after");
    SiteTemplateRenderer renderer(book);
    require(renderer.load(&error), "load relative custom theme");
    require(renderer.assetsPath() == temp.filePath("theme/source") && renderer.contains("index") && !renderer.contains("archive"), "assets and optional templates");
    require(renderer.render("index", {{"author", ""}, {"city", ""}, {"body", "<b>body</b>"}, {"title", "&lt;title&gt;"}}) == "before||<b>body</b>|&lt;title&gt;after", "empty conditions removed and caller escaping preserved");
    require(renderer.render("index", {{"author", "{{title}}"}, {"city", "city"}, {"body", "{{#city}}literal{{/city}}"}, {"title", "title"}}) == "beforeA:{{title}}|C:city|{{#city}}literal{{/city}}|titleafter", "substitution is single pass without evaluating inserted values");
    for (const auto &source: {QByteArray("{{#author}}nested{{#city}}bad{{/city}}{{/author}}"), QByteArray("{{/author}}"), QByteArray("{{author }"), QByteArray("{{unsupported}}")}) {
        write(path, source);
        SiteTemplateRenderer invalid(book);
        error.clear();
        require(!invalid.load(&error) && error.contains(path), "invalid template reports its path");
    }
    book.config["themeDirectory"] = "";
    SiteTemplateRenderer missing(book);
    error.clear();
    require(!missing.load(&error) && !error.isEmpty(), "missing custom directory");
    for (const auto &theme: {QString("pure"), QString("next"), QString("yilia"), QString("3-jekyll"), QString("unknown")}) {
        book.config["theme"] = theme;
        SiteTemplateRenderer builtIn(book);
        error.clear();
        require(builtIn.load(&error) && !builtIn.render("page", {{"title", "title"}, {"body", "body"}}).isEmpty(), "built-in template loading and unknown fallback");
    }
    qInfo() << "PASS: template loading, optional templates, relative paths, conditional blocks, single-pass substitution, invalid syntax and theme fallback";
}
