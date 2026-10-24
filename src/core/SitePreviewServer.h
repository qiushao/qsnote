#pragma once

#include <QTcpServer>

class SitePreviewServer : public QTcpServer {
public:
    SitePreviewServer(const QString &directory, const QString &root, QObject *parent = nullptr);
};
