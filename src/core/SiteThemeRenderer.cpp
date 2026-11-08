#include "SiteThemeRenderer.h"
#include "NotebookCategories.h"
#include "StaticSite.h"
#include "SiteDocuments.h"
#include "SiteNavigationIcons.h"
#include <QDate>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QUrl>

namespace {
QString escaped(const QString &text) { return text.toHtmlEscaped(); }
QString urlPath(const QString &path) { return QString::fromLatin1(QUrl::toPercentEncoding(path, "/")); }
QString json(const QJsonObject &object) {
    auto value = QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
    return value.replace('<', "\\u003c").replace('>', "\\u003e").replace('&', "\\u0026");
}

QString comments(const Notebook &book, const SitePage &page, const QString &permalink) {
    if (!page.post || !page.comments) return {};
    const auto &c = book.config;
    const auto platform = c["commentPlatform"].toString();
    if (platform == "giscus") {
        const auto repo = c["giscusRepo"].toString().trimmed();
        const auto repoId = c["giscusRepoId"].toString().trimmed();
        const auto category = c["giscusCategory"].toString().trimmed();
        const auto categoryId = c["giscusCategoryId"].toString().trimmed();
        if (repo.isEmpty() || repoId.isEmpty() || category.isEmpty() || categoryId.isEmpty()) return {};
        return R"(<section class="giscus"></section><script src="https://giscus.app/client.js" data-repo=")" + escaped(repo) +
               R"(" data-repo-id=")" + escaped(repoId) + R"(" data-category=")" + escaped(category) + R"(" data-category-id=")" + escaped(categoryId) +
               R"(" data-mapping="pathname" data-strict="0" data-reactions-enabled="1" data-emit-metadata="0" data-input-position="bottom" data-theme="preferred_color_scheme" data-lang="zh-CN" crossorigin="anonymous" async></script>)";
    }
    if (platform == "twikoo") {
        const auto envId = c["twikooEnvId"].toString().trimmed();
        if (envId.isEmpty()) return {};
        QJsonObject options{{"envId", envId}, {"el", "#tcomment"}, {"lang", "zh-CN"}};
        const auto region = c["twikooRegion"].toString().trimmed();
        if (!region.isEmpty()) options["region"] = region;
        return R"(<section id="tcomment"></section><script src="https://cdn.jsdelivr.net/npm/twikoo@2.0.17/dist/twikoo.all.min.js"></script><script>twikoo.init()" + json(options) + ");</script>";
    }
    if (platform == "disqus") {
        const auto shortname = c["disqus"].toString().trimmed();
        if (!QRegularExpression("\\A[A-Za-z0-9-]+\\z").match(shortname).hasMatch()) return {};
        QJsonObject options{{"identifier", page.route}, {"title", page.title}};
        const QUrl url(permalink);
        if (url.isValid() && !url.host().isEmpty() && (url.scheme() == "https" || url.scheme() == "http")) options["url"] = permalink;
        return "<section id=\"disqus_thread\"></section><script>var disqus_config=function(){var page=" + json(options) +
               ";this.page.identifier=page.identifier;this.page.title=page.title;if(page.url)this.page.url=page.url;};</script>"
               "<script src=\"https://" + shortname + ".disqus.com/embed.js\" defer></script>";
    }
    return {};
}

}// namespace

SiteThemeRenderer::SiteThemeRenderer(const Notebook &book, const SiteTemplateRenderer &templates, const SiteIndex &index)
    : book_(book), templates_(templates), index_(index), root_(StaticSite::rootPath(book)) {}

bool SiteThemeRenderer::plumeTheme() const { return templates_.theme() == SiteTemplateRenderer::Theme::Plume; }
bool SiteThemeRenderer::nextTheme() const { return templates_.theme() == SiteTemplateRenderer::Theme::Next; }
bool SiteThemeRenderer::yiliaTheme() const { return templates_.theme() == SiteTemplateRenderer::Theme::Yilia; }
bool SiteThemeRenderer::threeTheme() const { return templates_.theme() == SiteTemplateRenderer::Theme::Three; }
bool SiteThemeRenderer::shiroTheme() const { return templates_.theme() == SiteTemplateRenderer::Theme::Shiro; }
bool SiteThemeRenderer::customTheme() const { return templates_.theme() == SiteTemplateRenderer::Theme::Custom; }
QString SiteThemeRenderer::href(const QString &route) const { return urlPath(root_ + route); }
QString SiteThemeRenderer::themeTemplate(const QString &name, QMap<QString, QString> values) const {
    values["root"] = escaped(href({}));
    values["siteTitle"] = escaped(book_.config["title"].toString(book_.name));
    values["author"] = escaped(book_.config["author"].toString());
    values["city"] = escaped(book_.config["city"].toString().trimmed());
    const auto github = book_.config["github"].toString().trimmed();
    const QUrl githubUrl(github);
    values["githubSocial"] = githubUrl.scheme() == "https" || githubUrl.scheme() == "http" ? escaped(github) : QString();
    auto avatar = book_.config["avatar"].toString().trimmed();
    if (avatar.isEmpty() && !yiliaTheme() && !shiroTheme() && !customTheme() && !plumeTheme())
        avatar = nextTheme() ? "images/avatar.gif" : threeTheme() ? "assets/img/avatar.jpg"
                                                                  : "images/avatar.jpg";
    const QUrl avatarUrl(avatar);
    if (!avatar.isEmpty() && avatarUrl.isRelative() && (!avatar.startsWith('/') || (plumeTheme() && avatarUrl.host().isEmpty())))
        avatar = href(avatarUrl.path(QUrl::FullyDecoded).mid(avatar.startsWith('/') ? 1 : 0)) + (avatarUrl.hasQuery() ? "?" + avatarUrl.query(QUrl::FullyEncoded) : QString()) + (avatarUrl.hasFragment() ? "#" + avatarUrl.fragment(QUrl::FullyEncoded) : QString());
    if (plumeTheme()) {
        values["tagCount"] = QString::number(index_.tags.size());
        values["categoryCount"] = QString::number(index_.categories.size());
        values["postCount"] = QString::number(index_.posts.size());
        values["githubIcon"] = R"(<svg viewBox="0 0 24 24" aria-hidden="true" fill="currentColor"><path d="M12 .3a12 12 0 0 0-3.8 23.4c.6.1.8-.3.8-.6v-2.2c-3.3.7-4-1.4-4-1.4-.5-1.4-1.3-1.8-1.3-1.8-1.1-.8.1-.8.1-.8 1.2.1 1.8 1.2 1.8 1.2 1.1 1.9 2.8 1.3 3.5 1 .1-.8.4-1.3.8-1.6-2.7-.3-5.5-1.3-5.5-5.9 0-1.3.5-2.4 1.2-3.2-.1-.3-.5-1.5.1-3.2 0 0 1-.3 3.3 1.2a11.5 11.5 0 0 1 6 0c2.3-1.5 3.3-1.2 3.3-1.2.6 1.7.2 2.9.1 3.2.8.8 1.2 1.9 1.2 3.2 0 4.6-2.8 5.6-5.5 5.9.4.4.8 1.1.8 2.2v3.3c0 .3.2.7.8.6A12 12 0 0 0 12 .3Z"/></svg>)";
    }
    values["avatar"] = escaped(avatar);
    values["avatarImage"] = avatar.isEmpty() ? QString() : R"(<img class="show" src=")" + escaped(avatar) + "\" alt=\"" + values["author"] + R"(" width="128" height="128">)";
    values["description"] = escaped(book_.config["description"].toString());
    values["footer"] = book_.config["footer"].toString(R"(Powered by <a href="https://github.com/qiushao/qsnote">QSNote</a>)");
    values["domain"] = escaped(book_.config["domain"].toString(href({})));
    return templates_.render(name, values);
}

QString SiteThemeRenderer::link(const QString &route, const QString &title, const QString &classes) const {
    return "<a class=\"" + classes + "\" href=\"" + escaped(href(route)) + "\">" + escaped(title) + "</a>";
}

QString SiteThemeRenderer::taxonomyLinks(const SitePage &page, bool tags) const {
    QString result;
    if (yiliaTheme()) {
        for (const auto &name: tags ? page.tags : page.categories)
            result += R"(<li class="article-tag-list-item">)" + link((tags ? "tags/" + name : "categories/" + NotebookCategories::slug(book_, name)) + "/", name, "article-tag-list-link color" + QString::number(name.size() % 5 + 1)) + "</li>";
        if (result.isEmpty()) return {};
        return "<div class=\"" + QString(tags ? "article-tag" : "article-category") + " tagcloud\"><i class=\"icon " +
               (tags ? "icon-price-tags" : "icon-book") + R"("></i><ul class="article-tag-list">)" + result + "</ul></div>";
    }
    for (const auto &name: tags ? page.tags : page.categories) {
        if (nextTheme() && tags) result += link("tags/" + name + "/", "# " + name);
        else
            result += link((tags ? "tags/" + name : "categories/" + NotebookCategories::slug(book_, name)) + "/", name, tags ? "article-tag-link-link" : "article-category-link");
    }
    return result;
}

QString SiteThemeRenderer::meta(const SitePage &page, bool card) const {
    const auto date = escaped(page.date.left(10));
    if (plumeTheme()) {
        QString result;
        if (!page.categories.isEmpty()) result += R"(<div class="category-list"><span class="icon vpi-folder" aria-hidden="true"></span>)" + taxonomyLinks(page) + "</div>";
        int words = 0;
        auto matches = QRegularExpression(QStringLiteral("[\\p{Han}]|[\\p{L}\\p{N}]+")).globalMatch(page.text);
        while (matches.hasNext()) { matches.next(); ++words; }
        result += R"(<div class="reading-time"><span class="icon vpi-books" aria-hidden="true"></span><span>约 )" + QString::number(words) + " 字</span><span>" + QString::number(qMax(1, (words + 299) / 300)) + " 分钟</span></div>";
        if (!page.tags.isEmpty()) {
            result += R"(<div class="tag-list"><span class="icon vpi-tag" aria-hidden="true"></span>)";
            for (const auto &tag: page.tags) {
                uint color = 0;
                for (const auto ch: tag) color = color * 31 + ch.unicode();
                result += link("tags/" + tag + "/", tag, "tag vp-tag-color-" + QString::number(color % 6));
            }
            result += "</div>";
        }
        return result + R"(<div class="create-time"><span class="icon vpi-clock" aria-hidden="true"></span><time datetime=")" + escaped(page.date) + "\">" + date + "</time></div>";
    }
    if (shiroTheme()) return R"(<div class="shiro-meta text-sm text-text-chrome"><time datetime=")" + escaped(page.date) + "\">" + date + "</time>" + taxonomyLinks(page) + "</div>";
    if (customTheme()) return "<time datetime=\"" + escaped(page.date) + "\">" + date + "</time>";
    if (yiliaTheme()) return R"(<a class="archive-article-date" href=")" + escaped(href(page.route)) +
                             R"("><time itemprop="datePublished" datetime=")" + escaped(page.date) + R"("><i class="icon-calendar icon"></i>)" + date + "</time></a>";
    if (threeTheme()) return R"(<span class="post-meta-span date"><time datetime=")" + escaped(page.date) + "\">" + date +
                             R"(</time></span><span class="post-meta-span tag">)" + taxonomyLinks(page, true) + "</span>";
    if (nextTheme()) {
        QString result = R"(<span class="post-meta-item">发表于 <time datetime=")" + escaped(page.date) + "\">" + date + "</time></span>";
        if (!card) result += R"(<span class="post-meta-item">更新于 <time datetime=")" + escaped(page.updated) + "\">" + escaped(page.updated.left(10)) + "</time></span>";
        if (!page.categories.isEmpty()) result += R"(<span class="post-meta-item">分类于 )" + taxonomyLinks(page) + "</span>";
        return result;
    }
    const auto displayDate = card ? QDate::fromString(page.date.left(10), Qt::ISODate).toString("M月 d") : page.date.left(10);
    QString result = R"(<span class="article-date"><i class="icon icon-calendar-check"></i> <a class="article-date" href=")" + escaped(href(page.route)) + "\"><time datetime=\"" + escaped(page.date) + R"(" itemprop="datePublished">)" + escaped(displayDate) + "</time></a></span>\n";
    if (!page.categories.isEmpty()) result += R"(<span class="article-category"><i class="icon icon-folder"></i> )" + taxonomyLinks(page) + "</span>\n";
    if (!page.tags.isEmpty()) result += R"(<span class="article-tag"><i class="icon icon-tags"></i> )" + taxonomyLinks(page, true) + "</span>\n";
    return result + R"(<span class="post-comment"><i class="icon icon-comment"></i> <a class="article-comment-link" href=")" + escaped(href(page.route)) + "#comments\">评论</a></span>";
}

QString SiteThemeRenderer::permalink(const QString &route) const {
    const QUrl domain(book_.config["domain"].toString());
    if (domain.host().isEmpty()) return href(route);
    return domain.scheme() + "://" + domain.authority() + href(route);
}

QString SiteThemeRenderer::article(const SitePage &page, bool index) const {
    if (customTheme()) return themeTemplate("post", {{"title", escaped(page.title)}, {"body", page.html}, {"url", escaped(href(page.route))}, {"permalink", escaped(permalink(page.route))}, {"date", escaped(page.date.left(10))}, {"meta", meta(page)}, {"tags", taxonomyLinks(page, true)}, {"categories", taxonomyLinks(page)}, {"navigation", postNavigation(page)}, {"comments", comments(book_, page, permalink(page.route))}, {"toc", page.toc.isEmpty() ? QString() : "<ol>" + page.toc + "</ol>"}});
    if (!nextTheme() && !yiliaTheme() && !threeTheme() && !shiroTheme()) {
        auto result = themeTemplate("post", {{"articleId", escaped("post-" + page.route)}, {"title", escaped(page.title)}, {"meta", meta(page)}, {"body", page.html}, {"url", escaped(href(page.route))}, {"permalink", escaped(permalink(page.route))}, {"comments", index ? QString() : comments(book_, page, permalink(page.route))}});
        if (index) result.replace(R"(<h1 class="article-title" itemprop="name">)" + escaped(page.title) + "</h1>", "<h1 itemprop=\"name\">" + link(page.route, page.title, "article-title") + "</h1>");
        return result;
    }
    return themeTemplate("post", {{"title", escaped(page.title)}, {"meta", meta(page)}, {"body", page.html}, {"tags", taxonomyLinks(page, true)}, {"categories", taxonomyLinks(page)}, {"navigation", postNavigation(page)}, {"comments", comments(book_, page, permalink(page.route))}, {"toc", page.toc.isEmpty() ? QString() : "<ol class=\"toc-list\">" + page.toc + "</ol>"}});
}

QString SiteThemeRenderer::postNavigation(const SitePage &page) const {
    const auto &pages = page.document ? index_.documents.value(page.collectionId) : index_.posts;
    const auto index = pages.indexOf(&page);
    const auto *previous = index > 0 ? pages[index - 1] : nullptr;
    const auto *next = index >= 0 && index + 1 < pages.size() ? pages[index + 1] : nullptr;
    if (plumeTheme()) return "<nav class=\"vp-doc-footer\" aria-label=\"上下篇\">" + (previous ? link(previous->route, "← " + previous->title) : QString("<span></span>")) + (next ? link(next->route, next->title + " →") : QString()) + "</nav>";
    if (customTheme()) return (previous ? link(previous->route, "‹ " + previous->title, "prev") : QString()) + (next ? link(next->route, next->title + " ›", "next") : QString());
    if ((nextTheme() || yiliaTheme() || threeTheme() || shiroTheme()) && !previous && !next) return {};
    if (shiroTheme()) return R"(<nav class="section-divider shiro-post-nav" aria-label="文章导航"><div>)" +
                             (previous ? link(previous->route, "← " + previous->title, "post-nav-link") : QString()) + "</div><div>" +
                             (next ? link(next->route, next->title + " →", "post-nav-link") : QString()) + "</div></nav>";
    if (yiliaTheme()) {
        QString result = R"(<nav id="article-nav">)";
        if (previous) result += R"(<a id="article-nav-newer" class="article-nav-link-wrap" href=")" + escaped(href(previous->route)) +
                                R"("><i class="icon-circle-left"></i><div class="article-nav-title">)" + escaped(previous->title) + "</div></a>";
        if (next) result += R"(<a id="article-nav-older" class="article-nav-link-wrap" href=")" + escaped(href(next->route)) +
                            R"("><div class="article-nav-title">)" + escaped(next->title) + R"(</div><i class="icon-circle-right"></i></a>)";
        return result + "</nav>";
    }
    if (nextTheme() || threeTheme()) return R"(<div class="post-nav"><div class="post-nav-item">)" +
                                            (previous ? link(previous->route, "‹ " + previous->title) : QString()) + R"(</div><div class="post-nav-item">)" +
                                            (next ? link(next->route, next->title + " ›") : QString()) + "</div></div>";
    QString result = R"(<nav class="bar bar-footer clearfix" data-stick-bottom><div class="bar-inner"><ul class="pager pull-left">)";
    if (previous) result += R"(<li class="prev"><a href=")" + escaped(href(previous->route)) + "\" title=\"" + escaped(previous->title) + "\"><i class=\"icon icon-angle-left\"></i><span>  上一篇</span></a></li>";
    if (next) result += R"(<li class="next"><a href=")" + escaped(href(next->route)) + "\" title=\"" + escaped(next->title) + "\"><span>下一篇  </span><i class=\"icon icon-angle-right\"></i></a></li>";
    if (!page.toc.isEmpty()) result += R"(<li class="toggle-toc"><a class="toggle-btn collapsed" data-toggle="collapse" href="#collapseToc" aria-expanded="false" aria-controls="collapseToc" title="Catalogue" role="button"><span>[&nbsp;</span><span>Catalogue</span> <i class="text-collapsed icon icon-anchor"></i><i class="text-in icon icon-close"></i><span>]</span></a></li>)";
    return result + "</ul><div class=\"bar-right\"></div></div></nav>";
}

QString SiteThemeRenderer::collection(const QList<const SitePage *> &posts) const {
    QString result;
    for (const auto *post: posts) result += R"(<a class="collection-item" itemprop="url" href=")" + escaped(href(post->route)) + "\"><time datetime=\"" + escaped(post->date) + R"(" itemprop="datePublished">)" + escaped(post->date.left(10)) + "</time> <span>   </span> " + escaped(post->title) + "</a>";
    return result;
}

QStringList SiteThemeRenderer::categoryNames() const {
    QStringList names;
    for (const auto &name: NotebookCategories::site(book_)) {
        if (index_.categories.contains(name)) names.append(name);
    }
    names.append(index_.categories.keys());
    names.removeDuplicates();
    return names;
}

QString SiteThemeRenderer::taxonomy(const QString &kind, const QString &selected, const QList<const SitePage *> &subset) const {
    const auto &groups = kind == "categories" ? index_.categories : index_.tags;
    const auto names = kind == "categories" ? categoryNames() : groups.keys();
    const auto title = kind == "categories" ? "分类" : "标签";
    if (yiliaTheme() || threeTheme() || shiroTheme() || customTheme()) {
        QString terms;
        for (const auto &name: names)
            terms += "<li>" + link(kind + "/" + (kind == "categories" ? NotebookCategories::slug(book_, name) : name) + "/", name) + " (" + QString::number(groups.value(name).size()) + ")</li>";
        return themeTemplate("page", {{"title", title}, {"body", "<ul>" + terms + "</ul>"}});
    }
    QString body, nav = "<li>" + link(kind + "/", "All") + "</li>";
    for (const auto &name: names) {
        const auto url = kind + "/" + (kind == "categories" ? NotebookCategories::slug(book_, name) : name) + "/";
        nav += "<li>" + link(url, name) + "</li>";
        if (!selected.isEmpty() && name != selected) continue;
        const auto &posts = selected.isEmpty() ? groups.value(name) : subset;
        if (nextTheme()) {
            body += kind == "categories" ? R"(<li class="category-list-item">)" + link(url, name, "category-list-link") +
                                                   R"(<span class="category-list-count">)" + QString::number(posts.size()) + "</span></li>"
                                         : link(url, name) + " ";
        } else if (kind == "categories") {
            const auto id = "collapse" + escaped(name);
            body += R"(<div class="panel panel-default b-no"><div class="panel-heading" role="tab"><h3 class="panel-title"><a data-toggle="collapse" href="#)" + id + R"(" aria-expanded="true"><i class="icon icon-folder text-active"></i><i class="icon icon-folder-open text"></i> )" + escaped(name) + "</a> <small class=\"text-muted\">(Total " + QString::number(posts.size()) + " articles)</small></h3></div><div id=\"" + id + R"(" class="panel-collapse collapse in" role="tabpanel"><div class="panel-body"><div class="collection">)" + collection(posts) + "</div></div></div></div>";
        } else {
            body += "<h3 class=\"panel-title mb-1x\">" + link(url, "# " + name) + " <small class=\"text-muted\">(Total " + QString::number(posts.size()) + " articles)</small></h3><div class=\"row\">";
            for (const auto *post: posts) body += R"(<div class="col-md-6"><article class="panel panel-default hover-shadow hover-grow" itemscope itemtype="http://schema.org/BlogPosting"><div class="panel-body"><div class="article-meta"><time>)" + escaped(post->date.left(10)) + R"(</time></div><h3 class="article-title" itemprop="name">)" + link(post->route, post->title, "article-link") + "</h3></div><div class=\"panel-footer\">" + tagLabels(*post) + "</div></article></div>";
            body += "</div>";
        }
    }
    if (nextTheme()) return themeTemplate("page", {{"title", title}, {"body", kind == "categories" ? R"(<ul class="category-list">)" + body + "</ul>" : R"(<div class="tag-cloud"><div class="tag-cloud-tags">)" + body + "</div></div>"}});
    return "<article class=\"article article-" + kind + R"( article-type-list"><header class="article-header"><h1 class="hidden-xs" itemprop="name">)" + title + (selected.isEmpty() ? QString() : ": " + escaped(selected)) + "</h1><p class=\"text-muted hidden-xs\">共 " + QString::number(selected.isEmpty() ? groups.size() : subset.size()) + (selected.isEmpty() ? " 个" + QString(title) : " 篇文章") + R"(</p><nav role="navigation" id="nav-main" class="okayNav"><ul>)" + nav + "</ul></nav></header><div class=\"article-body\">" + body + "</div></article>";
}

QString SiteThemeRenderer::tagLabels(const SitePage &page) const {
    QString result;
    for (const auto &tag: page.tags.mid(0, 3)) result += link("tags/" + tag + "/", tag, "label label-default mb") + " ";
    return result;
}

QString SiteThemeRenderer::sidebarRecent() const {
    QString result;
    auto recent = index_.posts;
    for (const auto *page: recent.mid(0, 5)) {
        result += R"(<li><div class="item-inner"><p class="item-category">)" + taxonomyLinks(*page).replace("article-category-link", "category-link") + "</p><p class=\"item-title\">" + link(page->route, page->title, "title") + R"(</p><p class="item-date"><time datetime=")" + escaped(page->date) + R"(" itemprop="datePublished">)" + escaped(page->date.left(10)) + "</time></p></div></li>";
    }
    return result;
}

QString SiteThemeRenderer::sidebarTerms(const QMap<QString, QList<const SitePage *>> &groups, const QString &type, bool cloud) const {
    QString result;
    const auto names = type == "category" ? categoryNames() : groups.keys();
    for (const auto &name: names) {
        const auto url = (type == "category" ? "categories/" + NotebookCategories::slug(book_, name) : type + "s/" + name) + "/";
        if (cloud) result += "<a href=\"" + escaped(href(url)) + R"(" style="font-size: 13px;">)" + escaped(name) + "</a> ";
        else
            result += "<li class=\"" + type + "-list-item\">" + link(url, name, type + "-list-link") + "<span class=\"" + type + "-list-count\">" + QString::number(groups.value(name).size()) + "</span></li>";
    }
    return result;
}

QString SiteThemeRenderer::injectCode(QString markup) const {
    const auto headEnd = markup.lastIndexOf("</head>", -1, Qt::CaseInsensitive);
    const auto bodyEnd = markup.lastIndexOf("</body>", -1, Qt::CaseInsensitive);
    QString bodyCode;
    for (const auto &key: {"analyticsCode", "customBodyCode"}) {
        const auto code = book_.config[key].toString();
        if (!code.isEmpty()) bodyCode += code + '\n';
    }
    if (bodyEnd >= 0 && !bodyCode.isEmpty()) markup.insert(bodyEnd, bodyCode);
    const auto headCode = book_.config["customHeadCode"].toString();
    if (headEnd >= 0 && !headCode.isEmpty()) markup.insert(headEnd, headCode + '\n');
    return markup;
}

QString SiteThemeRenderer::html(const QString &route, const QString &title, const QString &body, const SitePage *page) const {
    auto keywords = page && !page->keywords.isEmpty() ? page->keywords : book_.config["keywords"].toString().trimmed();
    keywords.replace(QChar(0xff0c), QChar(','));
    if (plumeTheme()) {
        const bool document = page && page->document;
        const auto markup = themeTemplate("shell", {{"headTitle", escaped(title + " | " + book_.config["title"].toString(book_.name))},
            {"title", escaped(title)}, {"content", body}, {"menu", plumeNavigation(route)}, {"layout", document ? "document" : "blog"},
            {"sidebar", document ? plumeSidebar(page->collectionId, route) : themeTemplate("profile")},
            {"mobileProfile", document ? QString() : themeTemplate("profile")},
            {"toc", page && !page->toc.isEmpty() ? "<details open><summary>本页目录</summary><ol>" + page->toc + "</ol></details>" : QString()},
            {"keywordsMeta", keywords.isEmpty() ? QString() : R"(<meta name="keywords" content=")" + escaped(keywords) + "\">"}});
        return injectCode(markup);
    }
    if (customTheme()) {
        const auto markup = themeTemplate("layout", {{"headTitle", escaped(title + " | " + book_.config["title"].toString(book_.name))},
                                                     {"title", escaped(title)},
                                                     {"body", body},
                                                     {"permalink", escaped(permalink(route))},
                                                     {"keywords", escaped(keywords)},
                                                     {"articleStyle", R"(<link rel="stylesheet" href=")" + escaped(href("css/qsnote-article.css")) + "\">"}});
        return injectCode(markup);
    }
    QMap<QString, QString> values{{"headTitle", escaped(title + " | " + book_.config["title"].toString(book_.name))}, {"content", body}, {"keywordsMeta", keywords.isEmpty() ? QString() : R"(<meta name="keywords" content=")" + escaped(keywords) + "\">"}, {"title", escaped(title)}, {"permalink", escaped(permalink(route))}, {"sidebarRecent", sidebarRecent()}, {"sidebarCategories", sidebarTerms(index_.categories, "category")}, {"sidebarTags", sidebarTerms(index_.tags, "tag")}, {"sidebarCloud", sidebarTerms(index_.tags, "tag", true)}, {"layout", route.startsWith("archives/") || (!page && (route.startsWith("categories/") || route.startsWith("tags/"))) ? "archive posts-collapse" : page ? (page->post ? "post posts-expand" : "page posts-expand")
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                : "index posts-expand"},
                                  {"sidebarClass", page && page->post && !page->toc.isEmpty() ? "sidebar-nav-active sidebar-toc-active" : "sidebar-overview-active"},
                                  {"toc", page && page->post && !page->toc.isEmpty() ? R"(<div class="post-toc animated"><ol class="nav">)" + QString(page->toc).replace("class=\"toc-", "class=\"nav-") + "</ol></div>" : QString()},
                                  {"postCount", QString::number(index_.posts.size())},
                                  {"categoryCount", QString::number(index_.categories.size())},
                                  {"tagCount", QString::number(index_.tags.size())},
                                  {"config", json({{"root", href({})}, {"path", href("search.json")}, {"scheme", "Muse"}, {"sidebar", QJsonObject{{"position", "left"}, {"display", "post"}}}, {"motion", QJsonObject{{"enable", false}}}, {"codeblock", QJsonObject{{"fold", QJsonObject{{"enable", false}}}, {"copy_button", QJsonObject{{"enable", false}}}}}, {"comments", QJsonObject{{"lazyload", false}}}, {"localsearch", QJsonObject{{"preload", true}, {"top_n_per_article", 1}, {"unescape", false}}}, {"i18n", QJsonObject{{"hits", "找到 ${hits} 个搜索结果"}}}})},
                                  {"pageConfig", json({{"isHome", route.isEmpty()}, {"isPost", page && page->post}, {"lang", "zh-CN"}, {"sidebar", ""}})}};
    const QStringList months{"一月", "二月", "三月", "四月", "五月", "六月", "七月", "八月", "九月", "十月", "十一月", "十二月"};
    QString archives;
    for (auto it = index_.archives.cend(); it != index_.archives.cbegin();) {
        --it;
        if (it.key().size() != 8) continue;
        archives += "<li class=\"archive-list-item\">" + link("archives/" + it.key(), months.value(it.key().mid(5, 2).toInt() - 1) + " " + it.key().left(4), "archive-list-link") + "<span class=\"archive-list-count\">" + QString::number(it.value().size()) + "</span></li>";
    }
    values["sidebarArchives"] = archives;
    if (shiroTheme()) {
        QString menu;
        for (const auto &item: QList<QPair<QString, QString>>{{"", "首页"}, {"archives/", "归档"}, {"categories/", "分类"}, {"tags/", "标签"}, {"links/", "友链"}, {"about/", "关于"}})
            menu += "<li>" + link(item.first, item.second, "nav-desktop-link focus-elegant") + "</li>";
        values["menu"] = menu;
    } else if (threeTheme()) {
        QString filters, entries, menu;
        for (auto it = index_.tags.cbegin(); it != index_.tags.cend(); ++it)
            filters += R"(<li><button class="sidebar-tag" type="button" aria-pressed="false" data-filter=")" + escaped(it.key()) + "\">" + escaped(it.key()) + "</button></li>";
        for (const auto *post: index_.posts) {
            const auto tags = QString::fromUtf8(QJsonDocument(QJsonArray::fromStringList(post->tags)).toJson(QJsonDocument::Compact));
            entries += R"(<a class="toc-link" data-tags=")" + escaped(tags) + "\" href=\"" + escaped(href(post->route)) + "\"" +
                       (page == post ? QString(" aria-current=\"page\"") : QString()) + " title=\"" + escaped(post->title) + "\">" + escaped(post->title) + "</a>";
        }
        for (const auto &item: QList<QPair<QString, QString>>{{"", "首页"}, {"categories/", "分类"}, {"tags/", "标签"}, {"archives/", "归档"}, {"links/", "友链"}, {"about/", "关于"}})
            menu += link(item.first, item.second);
        values["filters"] = filters;
        values["entries"] = entries;
        values["menu"] = menu;
    } else if (yiliaTheme()) {
        QString menu;
        for (const auto &item: QList<QPair<QString, QString>>{{"", "主页"}, {"categories/", "分类"}, {"tags/", "标签"}, {"archives/", "归档"}, {"links/", "友链"}, {"about/", "关于"}})
            menu += "<li>" + link(item.first, item.second) + "</li>";
        values["menu"] = menu;
        values["toc"] = page && page->post && !page->toc.isEmpty() ? R"(<div class="toc-container tooltip-left" tabindex="0" aria-label="文章目录"><i class="icon-font icon-category"></i><div class="tooltip tooltip-east"><span class="tooltip-item"></span><div class="tooltip-content"><div class="toc-article"><ol class="toc">)" + page->toc + "</ol></div></div></div></div>" : QString();
    } else if (!nextTheme()) {
        values["tocSidebar"] = page && page->post && !page->toc.isEmpty() ? R"(<aside class="sidebar sidebar-toc collapse" id="collapseToc" itemscope itemtype="http://schema.org/WPSideBar"><div class="slimContent"><nav id="toc" class="article-toc"><h3 class="toc-title">Catalogue</h3><ol class="toc">)" + page->toc + "</ol></nav></div></aside>" : QString();
        values["categoryWidget"] = route == "about/" || index_.categories.isEmpty() ? QString() : themeTemplate("categoryWidget", values);
        values["tagWidget"] = route == "about/" || index_.tags.isEmpty() ? QString() : themeTemplate("tagWidget", values);
        values["cloudWidget"] = route == "about/" || index_.tags.isEmpty() ? QString() : themeTemplate("cloudWidget", values);
        values["archiveWidget"] = route == "about/" || index_.posts.isEmpty() ? QString() : themeTemplate("archiveWidget", values);
        values["recentWidget"] = route == "about/" || index_.posts.isEmpty() ? QString() : themeTemplate("recentWidget", values);
    }
    auto markup = themeTemplate("shell", values);
    markup.replace("</head>", R"(<link rel="stylesheet" href=")" + escaped(href("css/qsnote-article.css")) + "\"></head>");
    return injectCode(markup);
}

QString SiteThemeRenderer::pagination(const QString &route, int page, int count) const {
    if (plumeTheme()) {
        if (count == 1) return {};
        const auto pageRoute = [&route](int number) { return route + (number == 1 ? QString() : "page/" + QString::number(number) + "/"); };
        QString result = R"(<nav class="vp-pagination" aria-label="分页">)";
        result += page > 1 ? link(pageRoute(page - 1), "‹ 上一页", "page-range") : R"(<span class="page-range disabled" aria-disabled="true">‹ 上一页</span>)";
        result += "<div>";
        for (int i = 1; i <= count; ++i) result += i == page ? R"(<span class="current" aria-current="page">)" + QString::number(i) + "</span>" : link(pageRoute(i), QString::number(i));
        result += "</div>";
        result += page < count ? link(pageRoute(page + 1), "下一页 ›", "page-range") : R"(<span class="page-range disabled" aria-disabled="true">下一页 ›</span>)";
        return result + "</nav>";
    }
    if (nextTheme() || yiliaTheme() || threeTheme() || shiroTheme() || customTheme()) {
        if (count == 1) return {};
        QString result = yiliaTheme() ? R"(<nav id="page-nav">)" : R"(<nav class="pagination">)";
        if (page > 1) result += link(route + (page == 2 ? QString() : "page/" + QString::number(page - 1) + "/"), "‹", "extend prev");
        for (int i = 1; i <= count; ++i) result += i == page ? "<span class=\"page-number current\">" + QString::number(i) + "</span>" : link(route + (i == 1 ? QString() : "page/" + QString::number(i) + "/"), QString::number(i), "page-number");
        if (page < count) result += link(route + "page/" + QString::number(page + 1) + "/", "›", "extend next");
        return result + "</nav>";
    }
    return R"(<nav class="bar bar-footer clearfix" data-stick-bottom><div class="bar-inner"><ul class="pager pull-left"><li class="prev)" + QString(page == 1 ? " disabled" : "") + "\"><a href=\"" + (page == 1 ? "javascript:;" : escaped(href(route + (page == 2 ? QString() : "page/" + QString::number(page - 1) + "/")))) + "\"><i class=\"icon icon-angle-left\"></i> 上一页</a></li><li class=\"next" + QString(page == count ? " disabled" : "") + "\"><a href=\"" + (page == count ? "javascript:;" : escaped(href(route + "page/" + QString::number(page + 1) + "/"))) + "\">下一页 <i class=\"icon icon-angle-right\"></i></a></li></ul><div class=\"total-article bar-right\">Page " + QString::number(page) + " of " + QString::number(count) + "</div></div></nav>";
}

QString SiteThemeRenderer::renderPage(const SitePage &page) const {
    if (plumeTheme()) {
        QString content = page.html, breadcrumb;
        if (page.document) {
            const auto group = SiteDocuments::collection(book_, page.collectionId);
            breadcrumb = link("docs/" + group["slug"].toString() + '/', group["name"].toString());
            QStringList parents;
            const auto *note = SiteDocuments::find(book_, page.id);
            while (note && !note->parentId.isEmpty()) {
                note = SiteDocuments::find(book_, note->parentId);
                if (note) parents.prepend(escaped(note->title));
            }
            for (const auto &parent: parents) breadcrumb += " / " + parent;
        } else if (page.route == "categories/" || page.route == "tags/") {
            const bool tags = page.route == "tags/";
            const auto &groups = tags ? index_.tags : index_.categories;
            content += "<ul>";
            for (auto it = groups.cbegin(); it != groups.cend(); ++it) content += "<li>" + link(page.route + (tags ? it.key() : NotebookCategories::slug(book_, it.key())) + '/', it.key()) + " (" + QString::number(it.value().size()) + ")</li>";
            content += "</ul>";
        } else if (page.route == "links/") {
            content += "<ul>";
            for (const auto &value: book_.config["links"].toArray()) {
                const auto item = value.toObject();
                const QUrl url(item["url"].toString());
                if (url.scheme() == "http" || url.scheme() == "https") content += "<li><a href=\"" + escaped(url.toString()) + "\">" + escaped(item["name"].toString()) + "</a></li>";
            }
            content += "</ul>";
        }
        const auto body = themeTemplate("page", {{"title", escaped(page.title)}, {"body", content}, {"breadcrumb", breadcrumb},
            {"meta", page.post ? meta(page) : QString()}, {"navigation", (page.post || !page.id.isEmpty()) ? postNavigation(page) : QString()}, {"comments", comments(book_, page, permalink(page.route))}});
        return html(page.route, page.title, body, &page);
    }
    QString body;
    if (page.post) body = article(page) + (nextTheme() || yiliaTheme() || threeTheme() || shiroTheme() || customTheme() ? QString() : postNavigation(page));
    else if (page.route == "categories/" || page.route == "tags/")
        body = taxonomy(page.route.section('/', 0, 0)) + page.html;
    else {
        auto content = page.html;
        if (page.route == "links/") {
            QString friends;
            for (const auto &entry: book_.config["links"].toArray()) {
                const auto item = entry.toObject();
                const QUrl url(item["url"].toString());
                if (url.scheme() != "http" && url.scheme() != "https") continue;
                friends += nextTheme() || yiliaTheme() || threeTheme() || shiroTheme() || customTheme() ? "<li><a href=\"" + escaped(url.toString()) + "\">" + escaped(item["name"].toString()) + "</a></li>"
                                                                                        : themeTemplate("friend", {{"url", escaped(url.toString())}, {"title", escaped(item["name"].toString())}});
            }
            content += nextTheme() || yiliaTheme() || threeTheme() || shiroTheme() || customTheme() ? "<ul>" + friends + "</ul>" : themeTemplate("links", {{"links", friends}});
        }
        body = !nextTheme() && !yiliaTheme() && !threeTheme() && !shiroTheme() && !customTheme() && page.route == "links/" ? content : themeTemplate("page", {{"title", escaped(page.title)}, {"body", content}});
    }
    return html(page.route, page.title, body, &page);
}

QString SiteThemeRenderer::renderListing(const QString &route, const QString &title, const QList<const SitePage *> &posts,
                                         const QList<const SitePage *> &subset, int page, int count) const {
    QString body;
    if (plumeTheme()) {
        for (const auto *post: subset) body += themeTemplate("card", {{"title", escaped(post->title)}, {"url", escaped(href(post->route))}, {"meta", meta(*post)}, {"pinned", post->pinned ? "<span class=\"sticky\">TOP</span>" : QString()}, {"excerpt", post->excerpt}});
        const auto heading = route.isEmpty() ? QString() : "<h1>" + escaped(title) + "</h1>";
        body = "<section class=\"vp-post-list\" aria-label=\"博客文章\">" + heading + (body.isEmpty() ? "<p>暂无文章</p>" : body) + pagination(route, page, count) + "</section>";
        return html(route + (page == 1 ? QString() : "page/" + QString::number(page) + "/"), title, body);
    }
    if (customTheme()) {
        QString entries;
        for (const auto *post: subset) entries += themeTemplate("card", {{"title", escaped(post->title)}, {"url", escaped(href(post->route))}, {"meta", meta(*post, true)}, {"date", escaped(post->date.left(10))}, {"tags", taxonomyLinks(*post, true)}, {"categories", taxonomyLinks(*post)}, {"excerpt", escaped(post->text.size() > 128 ? post->text.left(125) + "..." : post->text)}});
        auto name = route.startsWith("categories/") ? QString("category") : route.startsWith("tags/") ? QString("tag")
                                                                    : route.startsWith("archives/")   ? QString("archive")
                                                                                                      : QString("index");
        if (!templates_.contains(name)) name = name == "category" || name == "tag" ? "archive" : "index";
        if (!templates_.contains(name)) name = "index";
        body = themeTemplate(name, {{"title", escaped(title)}, {"body", entries}, {"pagination", pagination(route, page, count)}, {"pageNumber", QString::number(page)}, {"pageCount", QString::number(count)}, {"postCount", QString::number(posts.size())}});
        return html(route + (page == 1 ? QString() : "page/" + QString::number(page) + "/"), title, body);
    }
    if (shiroTheme()) {
        const bool archive = route.startsWith("archives/") || route.startsWith("categories/") || route.startsWith("tags/");
        QString year;
        for (const auto *post: subset) {
            if (archive && year != post->date.left(4)) {
                if (!year.isEmpty()) body += "</div></section>";
                year = post->date.left(4);
                body += R"(<section><h2 class="section-heading">)" + link("archives/" + year + "/", year, "section-heading-link oldstyle-nums") + R"(</h2><div class="space-y-4">)";
            }
            body += themeTemplate(archive ? "archive" : "card", {{"title", escaped(post->title)}, {"url", escaped(href(post->route))}, {"meta", meta(*post, true)}, {"date", escaped(post->date.left(10))}, {"excerpt", escaped(post->text.size() > 200 ? post->text.left(197) + "..." : post->text)}});
        }
        if (archive && !year.isEmpty()) body += "</div></section>";
        if (body.isEmpty()) body = "<p class=\"text-center text-text-chrome\">暂无文章</p>";
        if (archive) body = themeTemplate("list", {{"title", escaped(title)}, {"body", body}, {"postCount", QString::number(posts.size())}});
    } else if (threeTheme()) {
        QString entries;
        for (const auto *post: subset) entries += themeTemplate("card", {{"title", escaped(post->title)}, {"url", escaped(href(post->route))}, {"meta", meta(*post, true)}, {"excerpt", escaped(post->text.size() > 128 ? post->text.left(125) + "..." : post->text)}});
        body = themeTemplate("page", {{"title", escaped(title)}, {"body", entries.isEmpty() ? QString("<p>暂无文章</p>") : entries}});
    } else if (route.startsWith("archives/") || ((nextTheme() || yiliaTheme()) && (route.startsWith("categories/") || route.startsWith("tags/")))) {
        QString sections;
        QMap<QString, QList<const SitePage *>> years;
        for (const auto *post: subset) years[post->date.left(4)].append(post);
        for (auto it = years.cend(); it != years.cbegin();) {
            --it;
            if (yiliaTheme()) {
                sections += R"(<section class="archives-wrap"><div class="archive-year-wrap">)" + link("archives/" + it.key() + "/", it.key(), "archive-year") + R"(</div><div class="archives">)";
                for (const auto *post: it.value()) sections += themeTemplate("archive", {{"title", escaped(post->title)}, {"url", escaped(href(post->route))}, {"meta", meta(*post, true)}, {"tags", taxonomyLinks(*post, true)}, {"categories", taxonomyLinks(*post)}});
                sections += "</div></section>";
            } else if (nextTheme()) {
                sections += R"(<div class="collection-year"><span class="collection-header">)" + escaped(it.key()) + "</span></div>";
                for (const auto *post: it.value()) sections += R"(<article><header class="post-header"><div class="post-meta-container"><time>)" + escaped(post->date.mid(5, 5)) +
                                                               R"(</time></div><div class="post-title">)" + link(post->route, post->title, "post-title-link") + "</div></header></article>";
            } else
                sections += R"(<section class="panel panel-default b-no"><div class="panel-heading" role="tab"><h3 class="panel-title"><a data-toggle="collapse" href="#collapse)" + escaped(it.key()) + R"(" aria-expanded="true"><i class="icon icon-calendar-plus text-active"></i><i class="icon icon-calendar-minus text"></i> )" + escaped(it.key()) + R"(</a></h3></div><div class="panel-collapse collapse in" id="collapse)" + escaped(it.key()) + R"("><div class="panel-body"><div class="collection">)" + collection(it.value()) + "</div></div></div></section>";
        }
        if (yiliaTheme()) body = sections.isEmpty() ? themeTemplate("page", {{"title", escaped(title)}, {"body", "暂无文章"}}) : sections;
        else
            body = nextTheme() ? R"(<div class="post-block"><div class="post-content"><div class="collection-title"><span class="collection-header">)" + escaped(title) + " · 共 " + QString::number(posts.size()) + " 篇文章</span></div>" + sections + "</div></div>" : R"(<article class="content article article-archives article-type-list"><header class="article-header"><h1 itemprop="title">)" + escaped(title) + "</h1><p class=\"text-muted\">共 " + QString::number(posts.size()) + " 篇文章</p></header><div class=\"article-body\">" + sections + "</div></article>";
    } else if (!nextTheme() && !yiliaTheme() && (route.startsWith("categories/") || route.startsWith("tags/")))
        body = taxonomy(route.section('/', 0, 0), title, subset);
    else if (!nextTheme() && !yiliaTheme() && subset.size() == 1)
        body = article(*subset.first(), true);
    else {
        body = nextTheme() || yiliaTheme() ? QString() : "<div class=\"content article-list\">";
        for (const auto *post: subset) body += themeTemplate("card", {{"title", escaped(post->title)}, {"url", escaped(href(post->route))}, {"meta", meta(*post, true)}, {"tags", taxonomyLinks(*post, true)}, {"categories", taxonomyLinks(*post)}, {"excerpt", escaped(post->text.size() > 128 ? post->text.left(125) + "..." : post->text)}});
        if (!nextTheme() && !yiliaTheme()) body += "</div>";
    }
    return html(route + (page == 1 ? QString() : "page/" + QString::number(page) + "/"), title, body + pagination(route, page, count));
}

QJsonObject SiteThemeRenderer::searchEntry(const SitePage &page) const {
    return {{"type", page.document ? "doc" : "post"}, {"collection", SiteDocuments::collection(book_, page.collectionId)["name"].toString()}, {"collectionId", page.collectionId}, {"title", nextTheme() ? escaped(page.title) : page.title}, {"url", href(page.route)}, {"text", page.text}, {"path", href(page.route)}, {"content", nextTheme() ? escaped(page.text) : page.text}};
}

QJsonObject SiteThemeRenderer::contentEntry(const SitePage &page) const {
    QJsonObject entry{{"title", page.title}, {"text", page.text}, {"path", yiliaTheme() ? urlPath(page.route) : page.route}, {"date", page.date}};
    for (const auto &kind: {QString("categories"), QString("tags")}) {
        QJsonArray terms;
        for (const auto &term: kind == "categories" ? page.categories : page.tags) {
            const auto slug = kind == "categories" ? NotebookCategories::slug(book_, term) : term;
            terms.append(QJsonObject{{"name", term}, {"slug", slug}, {"permalink", href(kind + "/" + slug + "/")}});
        }
        entry[kind] = terms;
    }
    return entry;
}

QString SiteThemeRenderer::plumeNavigation(const QString &route) const {
    auto entries = book_.config["navigation"].toArray();
    if (!book_.config.contains("navigation")) {
        entries.append(QJsonObject{{"title", "首页"}, {"type", "builtin"}, {"target", ""}});
        QJsonArray docs;
        for (const auto &value: book_.config["docCollections"].toArray()) {
            const auto group = value.toObject();
            docs.append(QJsonObject{{"title", group["name"]}, {"type", "collection"}, {"target", group["id"]}});
        }
        if (!docs.isEmpty()) entries.append(QJsonObject{{"title", "文档"}, {"type", "group"}, {"children", docs}});
        for (const auto &entry: QList<QPair<QString, QString>>{{"categories/", "分类"}, {"tags/", "标签"}, {"archives/", "归档"}, {"links/", "友链"}, {"about/", "关于"}})
            entries.append(QJsonObject{{"title", entry.second}, {"type", "builtin"}, {"target", entry.first}});
    }
    int iconIndex = 0;
    const auto navigationLabel = [&iconIndex](const QJsonObject &item) {
        return SiteNavigationIcons::markup(item, "qs-nav-icon-" + QString::number(++iconIndex) + '-') + "<span>" + escaped(item["title"].toString()) + "</span>";
    };
    const auto renderLink = [&](const QJsonObject &item) {
        const auto type = item["type"].toString();
        auto target = item["target"].toString();
        if (type == "note") { const auto *page = index_.byId.value(target); target = page ? page->route : QString(); }
        else if (type == "collection") target = "docs/" + SiteDocuments::collection(book_, target)["slug"].toString() + '/';
        else if (type == "path") target = QUrl(target).path().mid(1);
        const bool active = type != "external" && (route == target || (type == "collection" && route.startsWith(target)));
        QString url = type == "external" ? target : href(target);
        if (type == "path") {
            const QUrl original(item["target"].toString());
            if (original.hasQuery()) url += '?' + original.query(QUrl::FullyEncoded);
            if (original.hasFragment()) url += '#' + original.fragment(QUrl::FullyEncoded);
        }
        return "<a href=\"" + escaped(url) + "\"" + (active ? " aria-current=\"page\"" : "") + ">" + navigationLabel(item) + "</a>";
    };
    QString result;
    for (const auto &value: entries) {
        const auto item = value.toObject();
        if (item["type"] == QJsonValue("group")) {
            QString children;
            for (const auto &child: item["children"].toArray()) children += renderLink(child.toObject());
            result += "<details class=\"vp-nav-group\"><summary>" + navigationLabel(item) + "</summary><div>" + children + "</div></details>";
        } else result += renderLink(item);
    }
    return result;
}

QString SiteThemeRenderer::plumeSidebar(const QString &collectionId, const QString &route) const {
    QMap<QString, QString> children;
    const auto ordered = SiteDocuments::ordered(book_, collectionId);
    for (auto it = ordered.crbegin(); it != ordered.crend(); ++it) {
        const auto *note = *it;
        QString entry;
        if (note->folder) {
            const auto content = children.value(note->id);
            if (!content.isEmpty()) entry = "<li><details open><summary>" + escaped(note->title) + "</summary><ul>" + content + "</ul></details></li>";
        } else if (const auto *page = index_.byId.value(note->id)) {
            entry = "<li><a href=\"" + escaped(href(page->route)) + "\"" + (page->route == route ? " aria-current=\"page\"" : "") + ">" + escaped(page->title) + "</a></li>";
        }
        children[note->parentId].prepend(entry);
    }
    const auto group = SiteDocuments::collection(book_, collectionId);
    return "<details open class=\"vp-sidebar\"><summary>" + escaped(group["name"].toString()) + "</summary><nav aria-label=\"文档目录\">" + link("docs/" + group["slug"].toString() + '/', "集合目录") + "<ul>" + children.value(QString()) + "</ul></nav></details>";
}
