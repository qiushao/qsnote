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

QString SiteThemeRenderer::href(const QString &route) const { return urlPath(root_ + route); }
QString SiteThemeRenderer::permalink(const QString &route) const {
    const QUrl domain(book_.config["domain"].toString());
    return domain.host().isEmpty() ? href(route) : domain.scheme() + "://" + domain.authority() + href(route);
}

QJsonObject SiteThemeRenderer::pageData(const SitePage &page, bool details) const {
    auto data = listPages_.value(&page);
    if (data.isEmpty()) {
        data = QJsonObject{{"id", page.id}, {"title", page.title}, {"url", href(page.route)}, {"route", page.route},
            {"permalink", permalink(page.route)}, {"date", page.date}, {"updated", page.updated}, {"body", page.html},
            {"text", page.text}, {"toc", page.toc}, {"post", page.post}, {"document", page.document}, {"pinned", page.pinned}};
        QJsonArray categories, tags;
        for (const auto &name: page.categories) categories.append(QJsonObject{{"name", name}, {"url", href("categories/" + NotebookCategories::slug(book_, name) + '/')}});
        for (const auto &name: page.tags) {
            uint color = 0;
            for (const auto ch: name) color = color * 31 + ch.unicode();
            tags.append(QJsonObject{{"name", name}, {"url", href("tags/" + name + '/')}, {"color", int(color % 6)}, {"label", tags.size() < 3}});
        }
        data["categories"] = categories; data["tags"] = tags;
        data["hasCategories"] = !categories.isEmpty(); data["hasTags"] = !tags.isEmpty();
        data["showNavigation"] = page.post || !page.id.isEmpty();
        data["tagsJson"] = QString::fromUtf8(QJsonDocument(QJsonArray::fromStringList(page.tags)).toJson(QJsonDocument::Compact));
        int words = 0;
        auto matches = QRegularExpression(QStringLiteral("[\\p{Han}]|[\\p{L}\\p{N}]+")).globalMatch(page.text);
        while (matches.hasNext()) { matches.next(); ++words; }
        data["wordCount"] = words; data["readingMinutes"] = qMax(1, (words + 299) / 300);
        const auto options = templates_.options();
        const int length = options["excerptLength"].toInt(128);
        data["excerpt"] = options["excerptFormat"] == QJsonValue("html") ? page.excerpt : page.text.size() > length ? page.text.left(length - 3) + "..." : page.text;
        if (!details) listPages_.insert(&page, data);
    }
    if (details) {
        data["comments"] = comments(book_, page, permalink(page.route));
        const auto &pages = page.document ? index_.documents.value(page.collectionId) : index_.posts;
        const auto position = pages.indexOf(&page);
        if (position > 0) data["previous"] = QJsonObject{{"title", pages[position - 1]->title}, {"url", href(pages[position - 1]->route)}};
        if (position >= 0 && position + 1 < pages.size()) data["next"] = QJsonObject{{"title", pages[position + 1]->title}, {"url", href(pages[position + 1]->route)}};
        data["hasNavigation"] = data.contains("previous") || data.contains("next");
    }
    return data;
}

QJsonArray SiteThemeRenderer::postData(const QList<const SitePage *> &posts) const {
    QJsonArray result;
    for (const auto *post: posts) result.append(pageData(*post, false));
    return result;
}

QJsonArray SiteThemeRenderer::terms(const QString &kind, const QString &selected, const QList<const SitePage *> &subset) const {
    const auto &groups = kind == "categories" ? index_.categories : index_.tags;
    QStringList names;
    if (kind == "categories") for (const auto &name: NotebookCategories::site(book_)) if (groups.contains(name)) names.append(name);
    names.append(groups.keys()); names.removeDuplicates();
    QJsonArray result;
    for (const auto &name: names) {
        const auto &posts = selected.isEmpty() ? groups.value(name) : subset;
        result.append(QJsonObject{{"name", name}, {"url", href(kind + '/' + (kind == "categories" ? NotebookCategories::slug(book_, name) : name) + '/')},
            {"count", int(posts.size())}, {"posts", postData(posts)}, {"selected", selected.isEmpty() || selected == name}});
    }
    return result;
}

QJsonObject SiteThemeRenderer::context(const QString &route, const QString &title, const SitePage *page) const {
    auto data = page ? pageData(*page) : QJsonObject{};
    const auto options = templates_.options();
    auto avatar = book_.config["avatar"].toString().trimmed();
    if (avatar.isEmpty()) avatar = options["defaultAvatar"].toString();
    const QUrl avatarUrl(avatar);
    if (!avatar.isEmpty() && avatarUrl.isRelative() && (!avatar.startsWith('/') || (options["rootRelativeAvatar"].toBool() && avatarUrl.host().isEmpty())))
        avatar = href(avatarUrl.path(QUrl::FullyDecoded).mid(avatar.startsWith('/') ? 1 : 0)) + (avatarUrl.hasQuery() ? "?" + avatarUrl.query(QUrl::FullyEncoded) : QString()) + (avatarUrl.hasFragment() ? "#" + avatarUrl.fragment(QUrl::FullyEncoded) : QString());
    const auto github = book_.config["github"].toString().trimmed();
    const QUrl githubUrl(github);
    auto keywords = page && !page->keywords.isEmpty() ? page->keywords : book_.config["keywords"].toString().trimmed();
    keywords.replace(QChar(0xff0c), QChar(','));
    QJsonObject site{{"root", href({})}, {"title", book_.config["title"].toString(book_.name)}, {"author", book_.config["author"].toString()},
        {"city", book_.config["city"].toString().trimmed()}, {"githubSocial", githubUrl.scheme() == "http" || githubUrl.scheme() == "https" ? github : QString()},
        {"avatar", avatar}, {"description", book_.config["description"].toString()},
        {"footer", book_.config["footer"].toString(R"(Powered by <a href="https://github.com/qiushao/qsnote">QSNote</a>)")},
        {"domain", book_.config["domain"].toString(href({}))}, {"postCount", int(index_.posts.size())}, {"categoryCount", int(index_.categories.size())}, {"tagCount", int(index_.tags.size())},
        {"categories", terms("categories")}, {"tags", terms("tags")}, {"recent", postData(index_.posts.mid(0, 5))}, {"navigation", navigation(route)}};
    QJsonArray posts;
    for (const auto *post: index_.posts) { auto entry = pageData(*post, false); entry["active"] = post == page; posts.append(entry); }
    site["posts"] = posts;
    QJsonArray archives;
    for (auto it = index_.archives.cend(); it != index_.archives.cbegin();) {
        --it;
        if (it.key().size() == 8) archives.append(QJsonObject{{"url", href("archives/" + it.key())}, {"date", QString(it.key().left(7)).replace('/', '-') + "-01"}, {"count", int(it.value().size())}});
    }
    site["archives"] = archives;
    QJsonArray friends;
    for (const auto &value: book_.config["links"].toArray()) {
        const auto item = value.toObject(); const QUrl url(item["url"].toString());
        if (url.scheme() == "http" || url.scheme() == "https") friends.append(QJsonObject{{"title", item["name"]}, {"url", url.toString()}});
    }
    site["friends"] = friends;
    // Flat site fields keep the small shell templates readable; structured lists live under site.
    for (auto it = site.begin(); it != site.end(); ++it) if (!it.value().isArray()) data[it.key()] = it.value();
    data["siteTitle"] = site["title"]; data["site"] = site;
    data["title"] = title; data["headTitle"] = title + " | " + site["title"].toString(); data["keywords"] = keywords;
    data["permalink"] = permalink(route); data["url"] = href(route); data["route"] = route;
    const bool listing = !page;
    data["view"] = QJsonObject{{"home", route.isEmpty()}, {"index", listing && (route.isEmpty() || route.startsWith("page/"))},
        {"listing", listing}, {"archive", listing && route.startsWith("archives/")},
        {"taxonomy", listing && (route.startsWith("categories/") || route.startsWith("tags/"))},
        {"page", page && !page->post}, {"post", page && page->post}, {"document", page && page->document},
        {"about", route == "about/"}, {"links", route == "links/"}, {"categories", route == "categories/"}, {"tags", route == "tags/"}};
    data["postToc"] = page && page->post ? page->toc : QString();
    data["navToc"] = page && page->post ? QString(page->toc).replace("class=\"toc-", "class=\"nav-") : QString();
    const auto jsonString = [](const QString &value) {
        auto encoded = QString::fromUtf8(QJsonDocument(QJsonArray{value}).toJson(QJsonDocument::Compact));
        return encoded.mid(1, encoded.size() - 2).replace('<', "\\u003c").replace('>', "\\u003e").replace('&', "\\u0026");
    };
    data["rootJsonValue"] = jsonString(href({})); data["searchJsonValue"] = jsonString(href("search.json"));
    if (page && page->document) {
        const auto group = SiteDocuments::collection(book_, page->collectionId);
        data["collection"] = QJsonObject{{"title", group["name"]}, {"url", href("docs/" + group["slug"].toString() + '/')}};
        data["documentTree"] = documentTree(page->collectionId, route);
        QStringList parents;
        const auto *note = SiteDocuments::find(book_, page->id);
        while (note && !note->parentId.isEmpty()) { note = SiteDocuments::find(book_, note->parentId); if (note) parents.prepend(note->title); }
        data["parents"] = QJsonArray::fromStringList(parents);
    }
    if (page) data["page"] = pageData(*page);
    return data;
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

QString SiteThemeRenderer::renderPage(const SitePage &page) const {
    auto data = context(page.route, page.title, &page);
    const auto type = page.document ? "document" : page.post ? "post" : page.route == "categories/" ? "categories" : page.route == "tags/" ? "tags" : page.route == "links/" ? "links" : "page";
    data["body"] = page.html;
    data["content"] = templates_.renderEntry(type, data);
    return injectCode(templates_.renderEntry("layout", data));
}

QString SiteThemeRenderer::renderListing(const QString &route, const QString &title, const QList<const SitePage *> &posts,
                                         const QList<const SitePage *> &subset, int page, int count) const {
    const QString type = route.startsWith("archives/") ? "archive" : route.startsWith("categories/") ? "category" : route.startsWith("tags/") ? "tag" : "index";
    const auto pageRoute = [&](int number) { return route + (number == 1 ? QString() : "page/" + QString::number(number) + '/'); };
    auto data = context(pageRoute(page), title);
    data["subsetCount"] = int(subset.size()); data["listCount"] = int(posts.size()); data["posts"] = postData(subset);
    data["listTitle"] = !route.isEmpty();
    data["card"] = true;
    data["categoryListing"] = type == "category"; data["tagListing"] = type == "tag";
    data["terms"] = terms(type == "tag" ? "tags" : "categories", title, subset);
    QJsonArray groups;
    if (templates_.options()["groupByYear"].toArray().contains(type)) {
        QMap<QString, QList<const SitePage *>> years;
        for (const auto *post: subset) years[post->date.left(4)].append(post);
        for (auto it = years.cend(); it != years.cbegin();) { --it; groups.append(QJsonObject{{"year", it.key()}, {"url", href("archives/" + it.key() + '/')}, {"posts", postData(it.value())}}); }
    }
    data["groups"] = groups;
    QJsonArray numbers;
    for (int i = 1; i <= count; ++i) numbers.append(QJsonObject{{"number", i}, {"url", href(pageRoute(i))}, {"current", i == page}});
    data["pagination"] = QJsonObject{{"number", page}, {"count", count}, {"multiple", count > 1}, {"pages", numbers},
        {"previous", page > 1 ? href(pageRoute(page - 1)) : QString()}, {"next", page < count ? href(pageRoute(page + 1)) : QString()}};
    if (type == "index" && subset.size() == 1 && templates_.options()["singlePostIndex"].toBool()) {
        auto single = pageData(*subset.first()); single["comments"] = QString(); single["inlinePost"] = true; single["card"] = false;
        data["singlePost"] = single;
    }
    data["content"] = templates_.renderEntry(type, data);
    return injectCode(templates_.renderEntry("layout", data));
}

QJsonObject SiteThemeRenderer::searchEntry(const SitePage &page) const {
    QJsonObject entry{{"type", page.document ? "doc" : "post"}, {"collection", SiteDocuments::collection(book_, page.collectionId)["name"].toString()}, {"collectionId", page.collectionId}, {"title", page.title}, {"url", href(page.route)}, {"text", page.text}, {"path", href(page.route)}, {"content", page.text}};
    for (const auto &field: templates_.options()["searchEscapeFields"].toArray()) entry[field.toString()] = escaped(entry[field.toString()].toString());
    return entry;
}

QJsonObject SiteThemeRenderer::contentEntry(const SitePage &page) const {
    QJsonObject entry{{"title", page.title}, {"text", page.text}, {"path", page.route}, {"date", page.date}};
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

QJsonArray SiteThemeRenderer::navigation(const QString &route) const {
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
    std::function<QJsonArray(const QJsonArray &)> convert = [&](const QJsonArray &items) {
        QJsonArray result;
        for (const auto &value: items) {
            const auto item = value.toObject();
            const auto type = item["type"].toString();
            QJsonObject entry{{"title", item["title"]}, {"icon", SiteNavigationIcons::markup(item, "qs-nav-icon-" + QString::number(++iconIndex) + '-')}};
            if (type == "group") {
                entry["children"] = convert(item["children"].toArray());
                entry["group"] = true;
            } else {
                auto target = item["target"].toString();
                if (type == "note") { const auto *page = index_.byId.value(target); target = page ? page->route : QString(); }
                else if (type == "collection") target = "docs/" + SiteDocuments::collection(book_, target)["slug"].toString() + '/';
                else if (type == "path") target = QUrl(target).path().mid(1);
                entry["active"] = type != "external" && (route == target || (type == "collection" && route.startsWith(target)));
                auto url = type == "external" ? target : href(target);
                if (type == "path") {
                    const QUrl original(item["target"].toString());
                    if (original.hasQuery()) url += '?' + original.query(QUrl::FullyEncoded);
                    if (original.hasFragment()) url += '#' + original.fragment(QUrl::FullyEncoded);
                }
                entry["url"] = url;
            }
            result.append(entry);
        }
        return result;
    };
    return convert(entries);
}

QString SiteThemeRenderer::documentTree(const QString &collectionId, const QString &route) const {
    QMap<QString, QString> children;
    const auto ordered = SiteDocuments::ordered(book_, collectionId);
    for (auto it = ordered.crbegin(); it != ordered.crend(); ++it) {
        const auto *note = *it;
        QJsonObject entry{{"title", note->title}, {"folder", note->folder}, {"children", children.value(note->id)}};
        if (note->folder && entry["children"].toString().isEmpty()) continue;
        if (!note->folder) {
            const auto *page = index_.byId.value(note->id);
            if (!page) continue;
            entry["title"] = page->title; entry["url"] = href(page->route); entry["active"] = page->route == route;
        }
        children[note->parentId].prepend(templates_.renderEntry("documentNode", entry));
    }
    return children.value(QString());
}
