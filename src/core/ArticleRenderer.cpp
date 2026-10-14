#include "ArticleRenderer.h"
#include "PlantUmlRenderer.h"
#include <QEventLoop>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QRegularExpression>
#include <QTimer>
#include <QWebEnginePage>
#include <QWebEngineSettings>
#include <QWebEngineUrlRequestInterceptor>

namespace {
class ExportRequests : public QWebEngineUrlRequestInterceptor {
public:
    using QWebEngineUrlRequestInterceptor::QWebEngineUrlRequestInterceptor;
    void interceptRequest(QWebEngineUrlRequestInfo &info) override {
        // Authored images are packaged by the caller, not fetched by the renderer.
        if (info.resourceType() == QWebEngineUrlRequestInfo::ResourceTypeImage && info.requestUrl().scheme() != "data") info.block(true);
    }
};
QString json(const QJsonObject &object) {
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
}
QVariant evaluate(QWebEnginePage &page, const QString &script) {
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    auto result = std::make_shared<QVariant>();
    const QPointer<QEventLoop> guard(&loop);
    page.runJavaScript(script, [result, guard](const QVariant &value) {
        *result = value;
        if (guard) guard->quit();
    });
    timeout.start(60000);
    loop.exec(QEventLoop::ExcludeUserInputEvents);
    return *result;
}
QString mathStylesheet() {
    QFile file(":/web/vendor/katex/katex.min.css");
    if (!file.open(QIODevice::ReadOnly)) return {};
    auto css = QString::fromUtf8(file.readAll());
    const QRegularExpression sources(R"(src:url\(([^)]+\.woff2)\)[^}]+)");
    auto matches = sources.globalMatch(css);
    while (matches.hasNext()) {
        const auto match = matches.next();
        QFile font(":/web/vendor/katex/" + match.captured(1));
        if (!font.open(QIODevice::ReadOnly)) return {};
        css.replace(match.captured(0), "src:url(data:font/woff2;base64," + QString::fromLatin1(font.readAll().toBase64()) + ") format(\"woff2\")");
    }
    return css;
}
}

ArticleRenderer::ArticleRenderer() = default;
ArticleRenderer::~ArticleRenderer() = default;

QString ArticleRenderer::stylesheet() {
    QFile file(":/web/article.css");
    QFile highlight(":/web/highlight.css");
    if (!file.open(QIODevice::ReadOnly) || !highlight.open(QIODevice::ReadOnly)) return {};
    return QString::fromUtf8(file.readAll()).replace("@import url(\"highlight.css\");", QString::fromUtf8(highlight.readAll()));
}

QList<QDomElement> ArticleRenderer::elements(const QDomDocument &document) {
    QList<QDomElement> result;
    QDomNode node = document.documentElement();
    while (!node.isNull()) {
        if (node.isElement()) result.append(node.toElement());
        if (node.hasChildNodes()) { node = node.firstChild(); continue; }
        while (!node.isNull() && node.nextSibling().isNull()) node = node.parentNode();
        if (!node.isNull()) node = node.nextSibling();
    }
    return result;
}

QString ArticleRenderer::html(const QDomDocument &document) {
    auto markup = document.toString(-1);
    // XML empty non-void elements must also parse correctly as text/html.
    markup.replace(QRegularExpression(R"(<(div|article|span|a|p|section|aside|figure|figcaption|nav|ol|ul|li|pre|code|table|thead|tbody|tr|td|th)(\s[^<>]*?)?/>)"), R"(<\1\2></\1>)");
    return markup;
}

bool ArticleRenderer::render(const QString &markdown, QDomDocument &document, QString *error) {
    if (!browser_) {
        browser_ = std::make_unique<QWebEnginePage>();
        browser_->setUrlRequestInterceptor(new ExportRequests(browser_.get()));
        browser_->settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, false);
        browser_->settings()->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, false);
        QEventLoop loop;
        QTimer timeout;
        timeout.setSingleShot(true);
        bool loaded = false;
        QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
        QObject::connect(browser_.get(), &QWebEnginePage::loadFinished, &loop, [&](bool ok) { loaded = ok; loop.quit(); });
        timeout.start(60000);
        QFile preview(":/web/preview.html");
        if (!preview.open(QIODevice::ReadOnly)) { *error = "无法读取预览资源。"; return false; }
        auto html = QString::fromUtf8(preview.readAll());
        html.remove("<script src=\"qrc:///qtwebchannel/qwebchannel.js\"></script>");
        html.replace("<script src=\"preview.js\"></script>", "<script src=\"article-export.js\"></script>");
        browser_->setHtml(html, QUrl("qrc:/web/"));
        loop.exec(QEventLoop::ExcludeUserInputEvents);
        if (!loaded) { *error = "无法加载正文渲染器。"; return false; }
    }
    const auto prepared = evaluate(*browser_, "(function(input){try{return {plantUml:qsnoteExport.prepare(input.text)}}catch(error){return {error:'公式 / Markdown：'+error.message}}})("
                                   + json({{"text", markdown}}) + ")").toMap();
    if (prepared.isEmpty() || prepared.contains("error")) {
        *error = prepared.value("error", "正文渲染超时。").toString();
        return false;
    }
    QJsonObject plantUml;
    for (const auto &value: prepared.value("plantUml").toList()) {
        const auto source = value.toString();
        if (plantUml.contains(source)) continue;
        PlantUmlRenderer renderer;
        QEventLoop loop;
        QByteArray png;
        QString message;
        QObject::connect(&renderer, &PlantUmlRenderer::finished, &loop, [&](const QByteArray &data, const QString &failure) {
            png = data; message = failure; loop.quit();
        });
        QTimer::singleShot(0, &renderer, [&] { renderer.render(source); });
        loop.exec(QEventLoop::ExcludeUserInputEvents);
        if (!message.isEmpty()) { *error = "PlantUML：" + message; return false; }
        plantUml[source] = QString::fromLatin1(png.toBase64());
    }
    static const auto mathCss = mathStylesheet();
    if (mathCss.isEmpty()) { *error = "无法读取公式字体和样式。"; return false; }
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    bool finished = false;
    QObject::connect(browser_.get(), &QWebEnginePage::titleChanged, &loop, [&](const QString &title) {
        if (title == "article-export-ready") { finished = true; loop.quit(); }
    });
    timeout.start(60000);
    browser_->runJavaScript("document.title='rendering';qsnoteExport.finish(" + json(plantUml) + ","
                            + json({{"css", mathCss}}) + ".css)");
    loop.exec(QEventLoop::ExcludeUserInputEvents);
    if (!finished) { *error = "正文渲染超时：" + evaluate(*browser_, "window.exportStage").toString(); return false; }
    const auto result = evaluate(*browser_, "window.exportResult").toMap();
    if (result.contains("error")) { *error = result.value("error").toString(); return false; }
    if (!document.setContent(result.value("html").toString(), false)) { *error = "无法读取正文渲染结果。"; return false; }
    return true;
}
