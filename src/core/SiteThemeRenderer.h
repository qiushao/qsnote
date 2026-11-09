#ifndef QSNOTE_SITETHEMERENDERER_H
#define QSNOTE_SITETHEMERENDERER_H

#include "SitePage.h"
#include "SiteTemplateRenderer.h"
#include <QJsonArray>

class SiteThemeRenderer {
public:
    SiteThemeRenderer(const Notebook &book, const SiteTemplateRenderer &templates, const SiteIndex &index);
    QString renderPage(const SitePage &page) const;
    QString renderListing(const QString &route, const QString &title, const QList<const SitePage *> &posts,
                          const QList<const SitePage *> &subset, int page, int count) const;
    QJsonObject searchEntry(const SitePage &page) const;
    QJsonObject contentEntry(const SitePage &page) const;

private:
    QString href(const QString &route) const;
    QString permalink(const QString &route) const;
    QJsonObject pageData(const SitePage &page, bool details = true) const;
    QJsonArray postData(const QList<const SitePage *> &posts) const;
    QJsonArray terms(const QString &kind, const QString &selected = {}, const QList<const SitePage *> &subset = {}) const;
    QJsonObject context(const QString &route, const QString &title, const SitePage *page = nullptr) const;
    QJsonArray navigation(const QString &route) const;
    QString documentTree(const QString &collectionId, const QString &route) const;
    QString injectCode(QString markup) const;
    const Notebook &book_;
    const SiteTemplateRenderer &templates_;
    const SiteIndex &index_;
    QString root_;
    // Only index-owned list pages are cached; synthetic collection pages are temporary.
    mutable QMap<const SitePage *, QJsonObject> listPages_;
};

#endif
