#pragma once

#include <QDomDocument>
#include <QString>
#include <memory>

class QWebEnginePage;

// Uses the preview's Markdown, highlighting and graphics implementation.
class ArticleRenderer {
public:
    ArticleRenderer(const QString &notebookPath, const QString &cacheGroup);
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
    std::unique_ptr<QWebEnginePage> browser_;
};
