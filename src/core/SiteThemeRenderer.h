#ifndef QSNOTE_SITETHEMERENDERER_H
#define QSNOTE_SITETHEMERENDERER_H

#include "SitePage.h"
#include "SiteTemplateRenderer.h"

class SiteThemeRenderer {
public:
    SiteThemeRenderer(const Notebook &book, const SiteTemplateRenderer &templates, const SiteIndex &index);
    QString renderPage(const SitePage &page) const;
    QString renderListing(const QString &route, const QString &title, const QList<const SitePage *> &posts,
                          const QList<const SitePage *> &subset, int page, int count) const;
    QJsonObject searchEntry(const SitePage &page) const;
    QJsonObject contentEntry(const SitePage &page) const;

private:
    bool nextTheme() const;
    bool yiliaTheme() const;
    bool threeTheme() const;
    bool customTheme() const;
    QString href(const QString &route) const;
    QString themeTemplate(const QString &name, QMap<QString, QString> values = {}) const;
    QString link(const QString &route, const QString &title, const QString &classes = {}) const;
    QString taxonomyLinks(const SitePage &page, bool tags = false) const;
    QString meta(const SitePage &page, bool card = false) const;
    QString permalink(const QString &route) const;
    QString article(const SitePage &page, bool index = false) const;
    QString postNavigation(const SitePage &page) const;
    QString collection(const QList<const SitePage *> &posts) const;
    QString taxonomy(const QString &kind, const QString &selected = {}, const QList<const SitePage *> &subset = {}) const;
    QString tagLabels(const SitePage &page) const;
    QString sidebarRecent() const;
    QString sidebarTerms(const QMap<QString, QList<const SitePage *>> &groups, const QString &type, bool cloud = false) const;
    QString injectCode(QString markup) const;
    QString html(const QString &route, const QString &title, const QString &body, const SitePage *page = nullptr) const;
    QString pagination(const QString &route, int page, int count) const;
    const Notebook &book_;
    const SiteTemplateRenderer &templates_;
    const SiteIndex &index_;
    QString root_;
};

#endif
