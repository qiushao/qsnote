#include "ArticleRenderer.h"
#include "MathStylesheet.h"
#include "PlantUmlRenderer.h"
#include "WebTranslations.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QEventLoop>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTimer>
#include <QWebEnginePage>
#include <QWebEngineSettings>
#include <QWebEngineUrlRequestInterceptor>

namespace {
QByteArray rendererFingerprint() {
    // Include bundled scripts, styles and fonts so application upgrades invalidate old renders.
    static const auto resources = [] {
        QCryptographicHash hash(QCryptographicHash::Sha256);
        hash.addData(qVersion());
        QStringList paths;
        QDirIterator files(":/web", QDir::Files, QDirIterator::Subdirectories);
        while (files.hasNext()) paths.append(files.next());
        paths.sort();
        for (const auto &path: paths) {
            QFile file(path);
            if (file.open(QIODevice::ReadOnly)) {
                hash.addData(path.toUtf8());
                hash.addData(file.readAll());
            }
        }
        return hash.result();
    }();
    return resources + QCoreApplication::translate("Web", "zh-CN").toUtf8();
}

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
}

ArticleRenderer::ArticleRenderer(const QString &notebookPath, const QString &cacheGroup) {
    const auto notebookKey = QCryptographicHash::hash(QDir(notebookPath).absolutePath().toUtf8(), QCryptographicHash::Sha256).toHex();
    cacheDirectory_ = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
        + '/' + cacheGroup + '/' + QString::fromLatin1(notebookKey) + "/articles";
}
ArticleRenderer::~ArticleRenderer() = default;

QString ArticleRenderer::cachePath(const QString &documentId) const {
    return cacheDirectory_ + "/" + QString::fromLatin1(QCryptographicHash::hash(documentId.toUtf8(), QCryptographicHash::Sha256).toHex());
}

void ArticleRenderer::removeCache(const QString &documentId) {
    QFile::remove(cachePath(documentId));
}

void ArticleRenderer::clearCache() {
    QDir(QDir(cacheDirectory_).absoluteFilePath("..")).removeRecursively();
}

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

bool ArticleRenderer::render(const QString &markdown, const QString &documentId, QDomDocument &document, QString *error) {
    // Cache before callers resolve links, number chapters or package authored images.
    // PlantUML can include external files, so those articles must be rendered again.
    const bool cacheable = !markdown.contains("plantuml", Qt::CaseInsensitive) && !markdown.contains("puml", Qt::CaseInsensitive);
    const auto path = cachePath(documentId);
    const auto key = QCryptographicHash::hash("article-v1\n" + rendererFingerprint() + markdown.toUtf8(), QCryptographicHash::Sha256).toHex() + '\n';
    QFile cache(path);
    if (cacheable && cache.open(QIODevice::ReadOnly)) {
        const auto data = cache.readAll();
        const bool cached = data.startsWith(key) && document.setContent(data.mid(key.size()), false)
            && document.documentElement().tagName() == "article";
        cache.close();
        if (cached) return true;
    }
    if (!renderUncached(markdown, document, error)) return false;
    // Cache failures must not prevent preview or export.
    if (cacheable && QDir().mkpath(cacheDirectory_)) {
        QSaveFile output(path);
        const auto data = key + document.toByteArray(-1);
        if (output.open(QIODevice::WriteOnly) && output.write(data) == data.size()) output.commit();
    }
    return true;
}

bool ArticleRenderer::renderUncached(const QString &markdown, QDomDocument &document, QString *error) {
    if (!browser_) {
        browser_ = std::make_unique<QWebEnginePage>();
        installWebTranslations(browser_.get());
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
        if (!preview.open(QIODevice::ReadOnly)) { *error = QCoreApplication::translate("ArticleRenderer", "无法读取预览资源。"); return false; }
        auto html = QString::fromUtf8(preview.readAll());
        html.remove("<script src=\"qrc:///qtwebchannel/qwebchannel.js\"></script>");
        html.replace("<script src=\"preview.js\"></script>", "<script src=\"article-export.js\"></script>");
        browser_->setHtml(html, QUrl("qrc:/web/"));
        loop.exec(QEventLoop::ExcludeUserInputEvents);
        if (!loaded) { *error = QCoreApplication::translate("ArticleRenderer", "无法加载正文渲染器。"); return false; }
    }
    const auto prepared = evaluate(*browser_, "(function(input){try{return {plantUml:qsnoteExport.prepare(input.text)}}catch(error){return {error:qsnoteTranslations.mathError+error.message}}})("
                                   + json({{"text", markdown}}) + ")").toMap();
    if (prepared.isEmpty() || prepared.contains("error")) {
        *error = prepared.value("error", QCoreApplication::translate("ArticleRenderer", "正文渲染超时。")).toString();
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
    const auto mathCss = embeddedMathStylesheet(error);
    if (mathCss.isEmpty()) { *error = QCoreApplication::translate("ArticleRenderer", "无法读取公式字体和样式。") + "\n" + *error; return false; }
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
    if (!finished) { *error = QCoreApplication::translate("ArticleRenderer", "正文渲染超时：") + evaluate(*browser_, "window.exportStage").toString(); return false; }
    const auto result = evaluate(*browser_, "window.exportResult").toMap();
    if (result.contains("error")) { *error = result.value("error").toString(); return false; }
    if (!document.setContent(result.value("html").toString(), false)) { *error = QCoreApplication::translate("ArticleRenderer", "无法读取正文渲染结果。"); return false; }
    return true;
}
