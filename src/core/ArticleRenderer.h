#pragma once

#include <QDomDocument>
#include <QString>
#include <memory>

class QWebEnginePage;

// Uses the preview's Markdown, highlighting and graphics implementation.
class ArticleRenderer {
public:
    // foreground 为导出正文的前景色，用于栅格化的公式；同一条笔记按不同前景色分别缓存。
    ArticleRenderer(const QString &notebookPath, const QString &cacheGroup, QString foreground = {});
    ~ArticleRenderer();
    bool render(const QString &markdown, const QString &documentId, QDomDocument &document, QString *error);
    void removeCache(const QString &documentId);
    void clearCache();
    static QString stylesheet();
    static QList<QDomElement> elements(const QDomDocument &document);
    static QString html(const QDomDocument &document);

private:
    [[nodiscard]] QString cachePath(const QString &documentId) const;
    bool renderUncached(const QString &markdown, QDomDocument &document, QString *error);
    QString cacheDirectory_;
    QString foreground_;
    std::unique_ptr<QWebEnginePage> browser_;
};
