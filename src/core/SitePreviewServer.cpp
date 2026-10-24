#include "SitePreviewServer.h"
#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QTcpSocket>
#include <QUrl>

SitePreviewServer::SitePreviewServer(const QString &directory, const QString &root, QObject *parent) : QTcpServer(parent) {
    connect(this, &QTcpServer::newConnection, this, [this, directory, root] {
        while (hasPendingConnections()) {
            auto *socket = nextPendingConnection();
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            connect(socket, &QTcpSocket::readyRead, socket, [socket, directory, root] {
                auto request = socket->property("request").toByteArray() + socket->readAll();
                if (request.size() > 65536) {
                    socket->disconnectFromHost();
                    return;
                }
                if (!request.contains("\r\n\r\n")) {
                    socket->setProperty("request", request);
                    return;
                }
                const auto parts = request.left(request.indexOf("\r\n")).split(' ');
                auto path = parts.size() >= 2 ? QUrl::fromEncoded(parts[1]).path() : QString();
                const bool allowed = path.startsWith(root) && (parts[0] == "GET" || parts[0] == "HEAD");
                path = path.mid(root.size());
                if (path.isEmpty() || path.endsWith('/')) path += "index.html";
                const auto absolute = QFileInfo(directory + "/" + path).canonicalFilePath();
                const bool insideSite = allowed && absolute.startsWith(QFileInfo(directory).canonicalFilePath() + "/");
                if (insideSite && QFileInfo(absolute).isDir()) {
                    auto url = QUrl::fromEncoded(parts[1]);
                    url.setPath(url.path() + '/');
                    socket->write("HTTP/1.1 301 Moved Permanently\r\nLocation: " + url.toEncoded() + "\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
                    socket->disconnectFromHost();
                    return;
                }
                QFile file(absolute);
                const bool found = insideSite && file.open(QIODevice::ReadOnly);
                const auto body = found ? file.readAll() : QByteArray("Not found");
                const auto mime = found ? QMimeDatabase().mimeTypeForFile(absolute).name().toUtf8() : QByteArray("text/plain");
                socket->write(found ? "HTTP/1.1 200 OK\r\n" : "HTTP/1.1 404 Not Found\r\n");
                socket->write("Content-Type: " + mime + "\r\nContent-Length: " + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n");
                if (parts[0] != "HEAD") socket->write(body);
                socket->disconnectFromHost();
            });
        }
    });
}
