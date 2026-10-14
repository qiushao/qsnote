#pragma once

#include <QDomDocument>
#include <QString>
#include <memory>

class QWebEnginePage;

// Uses the preview's Markdown, highlighting and graphics implementation.
class ArticleRenderer {
public:
    ArticleRenderer();
    ~ArticleRenderer();
    bool render(const QString &markdown, QDomDocument &document, QString *error);
    static QString stylesheet();
    static QList<QDomElement> elements(const QDomDocument &document);
    static QString html(const QDomDocument &document);

private:
    std::unique_ptr<QWebEnginePage> browser_;
};
