#include "core/SitePreviewServer.h"
#include "core/StaticSite.h"
#include "ui/SitePreviewDialog.h"
#include <QTcpSocket>
#include <QWebEnginePage>
#include <QWebEngineView>
#include <QtWidgets>

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
    require(until([&] { return done; }), "JavaScript timeout");
    return result;
}
QByteArray request(quint16 port, const QByteArray &path, const QByteArray &method = "GET") {
    QTcpSocket socket;
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QByteArray result;
    QObject::connect(&socket, &QTcpSocket::connected, [&] { socket.write(method + ' ' + path + " HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n"); });
    QObject::connect(&socket, &QTcpSocket::readyRead, [&] { result += socket.readAll(); });
    QObject::connect(&socket, &QTcpSocket::disconnected, &loop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    socket.connectToHost(QHostAddress::LocalHost, port);
    timeout.start(5000);
    loop.exec();
    require(!result.isEmpty(), "HTTP request timeout: " + QString::fromUtf8(path));
    return result;
}
void write(const QString &path, const QByteArray &bytes) {
    require(QDir().mkpath(QFileInfo(path).absolutePath()), "fixture directory");
    QFile file(path);
    require(file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size(), "fixture write");
}
}// namespace
int main(int argc, char **argv) {
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QApplication app(argc, argv);
    QTemporaryDir temp;
    require(temp.isValid(), "temporary directory");
    qputenv("XDG_DATA_HOME", temp.path().toUtf8());
    qputenv("XDG_CACHE_HOME", temp.path().toUtf8());
    write(temp.filePath("public/index.html"), "Home");
    write(temp.filePath("public/dir/index.html"), "Directory");
    write(temp.filePath("secret.txt"), "secret");
    write(temp.filePath("public-sibling/secret.txt"), "secret");
    require(QFile::link(temp.filePath("secret.txt"), temp.filePath("public/link.txt")), "symlink fixture");
    SitePreviewServer server(temp.filePath("public"), "/blog/");
    require(server.listen(QHostAddress::LocalHost) && server.serverAddress().isLoopback(), "loopback listener");
    auto response = request(server.serverPort(), "/blog/");
    require(response.startsWith("HTTP/1.1 200") && response.endsWith("Home"), "GET home");
    response = request(server.serverPort(), "/blog/", "HEAD");
    require(response.contains("Content-Length: 4") && response.endsWith("\r\n\r\n"), "HEAD has length but no body");
    response = request(server.serverPort(), "/blog/dir?query=1");
    require(response.startsWith("HTTP/1.1 301") && response.contains("Location: /blog/dir/?query=1"), "directory redirect preserves query");
    require(request(server.serverPort(), "/blog/dir/").endsWith("Directory"), "directory index");
    for (const auto *path: {"/", "/blog2/", "/blog/missing", "/blog/../secret.txt", "/blog/%2e%2e/secret.txt", "/blog/../public-sibling/secret.txt", "/blog/link.txt"}) {
        require(request(server.serverPort(), path).startsWith("HTTP/1.1 404"), "reject out-of-site/missing path: " + QString(path));
    }
    require(request(server.serverPort(), "/blog/", "POST").startsWith("HTTP/1.1 404"), "only GET/HEAD");
    QWidget parent;
    parent.resize(1000, 700);
    parent.show();
    QString error;
    for (const auto &theme: {QString("pure"), QString("next"), QString("yilia"), QString("3-jekyll"), QString("custom")}) {
        NotebookStore store;
        store.notebooks = {{theme, "Preview", "site", {{"post", {}, "Preview Post", "# Body\nSearchable content", false, {{"slug", "post"}, {"date", "2026-10-24"}}}}, {{"theme", theme}, {"domain", "https://example.com/blog/"}, {"themeDirectory", QDir::current().filePath("examples/site-theme")}}, temp.filePath(theme)}};
        require(store.save(&error) && StaticSite::generate(store.notebooks.first(), &error), theme + ": " + error);
        for (int open = 0; open < 2; ++open) {
            quint16 port = 0;
            QPointer<QWebEngineView> browserGuard;
            QPointer<QTcpServer> serverGuard;
            {
                SitePreviewDialog dialog(store.notebooks.first(), &parent);
                auto *browser = dialog.findChild<QWebEngineView *>("siteBrowser");
                auto *listener = dialog.findChild<QTcpServer *>();
                browserGuard = browser;
                serverGuard = listener;
                bool loaded = false;
                QObject::connect(browser, &QWebEngineView::loadFinished, [&](bool ok) { loaded = ok; });
                require(dialog.start(&error), error);
                dialog.show();
                port = listener->serverPort();
                require(listener->serverAddress().isLoopback() && until([&] { return loaded; }), "preview loads " + theme);
                require(browser->url().path() == "/blog/" && evaluate(browser->page(), "document.body.innerText.includes('Preview Post')").toBool(), "theme homepage and subpath: " + theme);
                require(evaluate(browser->page(), "document.styleSheets.length").toInt() > 0, "theme styles loaded");
                const auto assets = evaluate(browser->page(), "[...document.querySelectorAll('link[rel=stylesheet],script[src],img[src]')].map(n=>n.href||n.src)").toList();
                for (const auto &asset: assets) {
                    const QUrl url(asset.toString());
                    if (url.host() == "127.0.0.1") require(request(port, url.path(QUrl::FullyEncoded).toUtf8()).startsWith("HTTP/1.1 200"), "local resource: " + url.toString());
                }
                require(request(port, "/blog/search.json").contains("Preview Post"), "search index available: " + theme);
                require(request(port, "/blog/post/").contains("Searchable content"), "article available: " + theme);
                dialog.findChild<QPushButton *>("closeSitePreview")->click();
            }
            require(browserGuard.isNull() && serverGuard.isNull(), "dialog releases browser and server");
            QTcpSocket probe;
            probe.connectToHost(QHostAddress::LocalHost, port);
            require(!probe.waitForConnected(500), "closed dialog leaves no listener");
        }
    }
    qInfo() << "PASS: GET/HEAD/redirects/path boundaries, five themes/subpaths/assets/search, browser/server lifetime and reopen";
}
