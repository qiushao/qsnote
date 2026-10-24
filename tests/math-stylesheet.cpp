#include "core/ArticleRenderer.h"
#include "core/MathStylesheet.h"
#include "ui/MarkdownEditor.h"
#include <QtWidgets>
#define private public
#include "ui/InplacePreview.h"
#undef private
void require(bool condition, const QString &message) {
    if (!condition) qFatal("%s", qPrintable(message));
}
void write(const QString &path, const QByteArray &data) {
    QFile file(path);
    require(file.open(QIODevice::WriteOnly) && file.write(data) == data.size(), "fixture write");
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
int main(int argc, char **argv) {
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QApplication app(argc, argv);
    QTemporaryDir temp;
    require(temp.isValid(), "temporary directory");
    qputenv("XDG_CACHE_HOME", temp.path().toUtf8());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temp.path());
    QString error;
    Q_CLEANUP_RESOURCE(vnote_web);
    require(embeddedMathStylesheet(&error).isEmpty() && error.contains("katex.min.css"), "missing stylesheet must fail");
    write(temp.filePath("style.css"), "@font-face{font-family:KaTeX;src:url(fonts/missing.woff2) format(\"woff2\")}");
    write(temp.filePath("test.qrc"), "<RCC><qresource prefix=\"/web/vendor/katex\"><file alias=\"katex.min.css\">style.css</file></qresource></RCC>");
    QProcess rcc;
    rcc.start(QLibraryInfo::path(QLibraryInfo::LibraryExecutablesPath) + "/rcc", {"--binary", "-o", temp.filePath("test.rcc"), temp.filePath("test.qrc")});
    require(rcc.waitForFinished() && rcc.exitCode() == 0, "compile test resource");
    require(QResource::registerResource(temp.filePath("test.rcc")), "register test resource");
    error.clear();
    require(embeddedMathStylesheet(&error).isEmpty() && error.contains("missing.woff2"), "missing font must fail without partial stylesheet");
    require(QResource::unregisterResource(temp.filePath("test.rcc")), "unregister test resource");
    Q_INIT_RESOURCE(vnote_web);
    error.clear();
    const auto css = embeddedMathStylesheet(&error);
    require(!css.isEmpty() && error.isEmpty(), "real stylesheet must load");
    require(!css.contains("url(fonts/") && !css.contains(".woff") && !css.contains(".ttf"), "no external font references");
    const QRegularExpression font("data:font/woff2;base64,([^)]+)");
    auto matches = font.globalMatch(css);
    int fonts = 0;
    while (matches.hasNext()) {
        require(QByteArray::fromBase64(matches.next().captured(1).toLatin1()).startsWith("wOF2"), "embedded WOFF2 bytes");
        ++fonts;
    }
    require(fonts > 0, "embedded fonts present");
    const QString markdown = "Inline $x^2$\n\n$$\\frac{1}{2}$$\n\n```mathjax\n\\sum_{n=1}^{10} n\n```\n";
    ArticleRenderer renderer(temp.path(), "math-regression");
    QDomDocument document;
    require(renderer.render(markdown, "math", document, &error), error);
    require(document.elementsByTagName("img").size() == 3, "export inline/display/fenced math");
    for (const auto &element: ArticleRenderer::elements(document)) {
        if (element.tagName() == "img") require(element.attribute("src").startsWith("data:image/png;base64,"), "self-contained math image");
    }
    MarkdownEditor editor;
    editor.resize(800, 600);
    editor.show();
    editor.setText(markdown);
    auto *inplace = editor.findChild<InplacePreview *>();
    require(inplace, "inplace renderer");
    require(until([&] {
                if (inplace->requests_.size() != 3) return false;
                for (const auto &request: inplace->requests_)
                    if (!request.item) return false;
                return true;
            }),
            "inplace inline/display/fenced math");
    qInfo() << "PASS: missing CSS/font errors, embedded WOFF2, inline/display/fenced math in export and inplace preview";
}
