#include "StaticSite.h"
#include <QCoreApplication>
#include "ArticleRenderer.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>
#include <QTemporaryDir>
#include <QUrl>
#include <algorithm>
#include <utility>

namespace {
struct Page {
    QString source;
    QString route;
    QString title;
    QString body;
    QString html;
    QString toc;
    QString text;
    QString date;
    QString updated;
    QStringList categories;
    QStringList tags;
    QString keywords;
    bool post = false;
    bool comments = true;
};

QString escaped(const QString &text) { return text.toHtmlEscaped(); }
QString urlPath(const QString &path) { return QString::fromLatin1(QUrl::toPercentEncoding(path, "/")); }
QString json(const QJsonObject &object) {
    auto value = QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
    return value.replace('<', "\\u003c").replace('>', "\\u003e").replace('&', "\\u0026");
}
QStringList strings(const QJsonValue &value) {
    QStringList result;
    if (value.isString()) result.append(value.toString());
    else for (const auto &entry: value.toArray()) if (entry.isString()) result.append(entry.toString());
    result.removeAll(QString());
    result.removeDuplicates();
    return result;
}

// QSNote writes JSON front matter. Accept the simple YAML fields used when authoring pages too.
bool frontMatter(QString &body, QJsonObject &metadata, QString *error) {
    body.replace("\r\n", "\n");
    if (!body.startsWith("---\n")) return true;
    const auto end = body.indexOf("\n---", 4);
    if (end < 0 || (end + 4 < body.size() && body[end + 4] != '\n')) {
        *error = QCoreApplication::translate("SiteGenerator", "front matter 缺少结束分隔符");
        return false;
    }
    const auto header = body.mid(4, end - 4).trimmed();
    if (header.startsWith('{')) {
        QJsonParseError parseError;
        const auto document = QJsonDocument::fromJson(header.toUtf8(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            *error = QCoreApplication::translate("SiteGenerator", "front matter JSON 无效：") + parseError.errorString();
            return false;
        }
        metadata = document.object();
    } else {
        QString listKey;
        const auto scalar = [](QString value) -> QJsonValue {
            value = value.trimmed();
            if (value == "true") return true;
            if (value == "false") return false;
            if (value.startsWith('"')) {
                const auto parsed = QJsonDocument::fromJson(("[" + value + "]").toUtf8());
                if (parsed.isArray()) return parsed.array().first();
            }
            if (value.startsWith('\'') && value.endsWith('\'')) return value.mid(1, value.size() - 2).replace("''", "'");
            return value;
        };
        for (const auto &line: header.split('\n')) {
            const auto trimmed = line.trimmed();
            if (trimmed.isEmpty() || trimmed.startsWith('#')) continue;
            if (trimmed.startsWith("- ") && !listKey.isEmpty()) {
                auto array = metadata[listKey].toArray();
                array.append(scalar(trimmed.mid(2)));
                metadata[listKey] = array;
                continue;
            }
            const auto match = QRegularExpression("^([A-Za-z_][A-Za-z0-9_]*):(?:\\s+(.*))?$").match(line);
            if (!match.hasMatch()) {
                *error = QCoreApplication::translate("SiteGenerator", "不支持的 front matter 行，请使用 JSON 或简单 YAML 字段：") + line;
                return false;
            }
            const auto key = match.captured(1);
            const auto value = match.captured(2).trimmed();
            listKey = value.isEmpty() ? key : QString();
            if (value.startsWith('[') && value.endsWith(']')) {
                QJsonArray array;
                for (const auto &item: value.mid(1, value.size() - 2).split(',', Qt::SkipEmptyParts)) array.append(scalar(item));
                metadata[key] = array;
            } else metadata[key] = scalar(value);
        }
    }
    body = body.mid(qMin(end + 5, body.size()));
    return true;
}



QString comments(const Notebook &book, const Page &page) {
    if (!page.post || !page.comments) return {};
    const auto &c = book.config;
    const auto platform = c["commentPlatform"].toString();
    if (platform == "valine") {
        return R"(<section id="vcomments"></section><script src="https://cdn.jsdelivr.net/npm/valine@1.5.1/dist/Valine.min.js"></script><script>new Valine()" +
            json({{"el", "#vcomments"}, {"appId", c["valineAppId"]}, {"appKey", c["valineAppKey"]}, {"visitor", false}}) + ");</script>";
    }
    if (platform == "gitalk") {
        return "<link rel=\"stylesheet\" href=\"https://cdn.jsdelivr.net/npm/gitalk@1.8.0/dist/gitalk.css\"><section id=\"gitalk-container\"></section>"
               "<script src=\"https://cdn.jsdelivr.net/npm/gitalk@1.8.0/dist/gitalk.min.js\"></script><script>new Gitalk(" +
            json({{"clientID", c["clientId"]}, {"clientSecret", c["clientSecret"]}, {"repo", c["commentRepo"]},
                  {"owner", c["commentOwner"]}, {"admin", QJsonArray{c["commentOwner"]}},
                  {"id", QString::fromLatin1(QCryptographicHash::hash(page.route.toUtf8(), QCryptographicHash::Md5).toHex())}}) + ").render('gitalk-container');</script>";
    }
    if (platform == "disqus" && QRegularExpression("^[A-Za-z0-9-]+$").match(c["disqus"].toString()).hasMatch()) {
        return "<section id=\"disqus_thread\"></section><script>var disqus_config=function(){this.page.identifier=" +
            json({{"id", page.route}}) + ".id;};</script><script src=\"https://" + c["disqus"].toString() + ".disqus.com/embed.js\" defer></script>";
    }
    return {};
}

class Generator {
public:
    Generator(const Notebook &book, QString output, QString *error)
        : book_(book), output_(std::move(output)), error_(error), root_(StaticSite::rootPath(book)) {}

    bool generate() {
        if (customTheme() && !loadCustomTheme()) return false;
        const auto source = book_.path + "/source";
        if (!QFileInfo(source).isDir()) return fail(QCoreApplication::translate("SiteGenerator", "找不到站点 source 目录"));
        QDirIterator files(source, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
        QStringList sources;
        while (files.hasNext()) sources.append(files.next());
        sources.sort();
        for (const auto &file: sources) {
            const auto relative = QDir(source).relativeFilePath(file);
            const auto segments = relative.split('/');
            bool excluded = false;
            for (const auto &segment: segments) {
                if (segment.startsWith('.') || (segment.startsWith('_') && segment != "_posts")) excluded = true;
            }
            if (excluded) continue;
            QFile input(file);
            if (!input.open(QIODevice::ReadOnly)) return fail(file + ": " + input.errorString());
            const auto data = input.readAll();
            if (!file.endsWith(".md", Qt::CaseInsensitive) && !file.endsWith(".markdown", Qt::CaseInsensitive)) {
                const auto target = relative.startsWith("_posts/") ? "assets/posts/" + relative.mid(7) : relative;
                links_[QDir::cleanPath(file)] = target;
                if (!write(target, data)) return false;
                continue;
            }
            Page page;
            page.source = file;
            page.body = QString::fromUtf8(data);
            QJsonObject metadata;
            if (!frontMatter(page.body, metadata, error_)) {
                *error_ = file + ": " + *error_;
                return false;
            }
            if (metadata["draft"].toBool() || metadata["published"] == QJsonValue(false)) continue;
            page.post = relative.startsWith("_posts/");
            page.comments = metadata["comments"] != QJsonValue(false);
            page.title = metadata["title"].toString(QFileInfo(file).completeBaseName());
            page.categories = strings(metadata["categories"]);
            page.tags = strings(metadata["tags"]);
            page.keywords = strings(metadata["keywords"]).join(", ").trimmed();
            page.date = metadata["date"].toString(QFileInfo(file).lastModified().toString(Qt::ISODate));
            page.updated = metadata["updated"].toString(QFileInfo(file).lastModified().toString(Qt::ISODate));
            auto route = metadata["permalink"].toString(metadata["slug"].toString());
            if (route.isEmpty()) route = page.post ? QFileInfo(file).completeBaseName() : relative.left(relative.lastIndexOf('.'));
            if (!page.post && (route == "index" || route.endsWith("/index"))) route.chop(5);
            if (!route.isEmpty() && !route.endsWith('/') && !route.endsWith(".html")) route += '/';
            page.route = route;
            if (!validPath(route.isEmpty() ? "index.html" : route)) return false;
            links_[QDir::cleanPath(file)] = page.route;
            pages_.append(page);
        }
        for (auto &page: pages_) if (!render(page)) return false;
        QList<const Page *> posts;
        QMap<QString, QList<const Page *>> categories, tags, archives;
        for (const auto &page: pages_) if (page.post) posts.append(&page);
        std::stable_sort(posts.begin(), posts.end(), [](const Page *a, const Page *b) { return a->date > b->date; });
        QJsonArray search;
        for (const auto *post: posts) {
            for (const auto &category: post->categories) categories[category].append(post);
            for (const auto &tag: post->tags) tags[tag].append(post);
            const auto date = QDate::fromString(post->date.left(10), Qt::ISODate);
            if (date.isValid()) {
                archives[date.toString("yyyy") + "/"].append(post);
                archives[date.toString("yyyy/MM") + "/"].append(post);
            }
            search.append(QJsonObject{{"title", nextTheme() ? escaped(post->title) : post->title}, {"url", href(post->route)}, {"text", post->text}, {"path", href(post->route)}, {"content", nextTheme() ? escaped(post->text) : post->text}});
        }
        allPosts_ = posts;
        categories_ = categories;
        tags_ = tags;
        archives_ = archives;
        if (!listing({}, book_.config["title"].toString(book_.name), posts) || !listing("archives/", "归档", posts)) return false;
        for (auto it = archives.cbegin(); it != archives.cend(); ++it) if (!listing("archives/" + it.key(), it.key(), it.value())) return false;
        for (const auto &group: {qMakePair(QString("categories"), categories), qMakePair(QString("tags"), tags)})
            for (auto it = group.second.cbegin(); it != group.second.cend(); ++it)
                if (!listing(group.first + "/" + it.key() + "/", it.key(), it.value())) return false;
        for (const auto &page: pages_) {
            QString body;
            if (page.post) body = article(page) + (nextTheme() || yiliaTheme() || threeTheme() || customTheme() ? QString() : postNavigation(page));
            else if (page.route == "categories/" || page.route == "tags/") body = taxonomy(page.route.section('/', 0, 0)) + page.html;
            else {
                auto content = page.html;
                if (page.route == "links/") {
                    QString friends;
                    for (const auto &entry: book_.config["links"].toArray()) {
                        const auto item = entry.toObject();
                        const QUrl url(item["url"].toString());
                        if (url.scheme() != "http" && url.scheme() != "https") continue;
                        friends += nextTheme() || yiliaTheme() || threeTheme() || customTheme() ? "<li><a href=\"" + escaped(url.toString()) + "\">" + escaped(item["name"].toString()) + "</a></li>"
                            : themeTemplate("friend", {{"url", escaped(url.toString())}, {"title", escaped(item["name"].toString())}});
                    }
                    content += nextTheme() || yiliaTheme() || threeTheme() || customTheme() ? "<ul>" + friends + "</ul>" : themeTemplate("links", {{"links", friends}});
                }
                body = !nextTheme() && !yiliaTheme() && !threeTheme() && !customTheme() && page.route == "links/" ? content : themeTemplate("page", {{"title", escaped(page.title)}, {"body", content}});
            }
            if (!html(page.route, page.title, body, &page)) return false;
        }
        if (!write("search.json", QJsonDocument(search).toJson(QJsonDocument::Compact))) return false;
        QJsonArray insightPosts, insightPages;
        for (const auto &page: pages_) {
            QJsonObject entry{{"title", page.title}, {"text", page.text}, {"path", yiliaTheme() ? urlPath(page.route) : page.route}, {"date", page.date}};
            for (const auto &kind: {QString("categories"), QString("tags")}) {
                QJsonArray terms;
                for (const auto &term: kind == "categories" ? page.categories : page.tags)
                    terms.append(QJsonObject{{"name", term}, {"slug", term}, {"permalink", href(kind + "/" + term + "/")}});
                entry[kind] = terms;
            }
            (page.post ? insightPosts : insightPages).append(entry);
        }
        if (!write("content.json", QJsonDocument(QJsonObject{{"posts", insightPosts}, {"pages", insightPages}}).toJson(QJsonDocument::Compact))) return false;
        if (!write("css/qsnote-article.css", ArticleRenderer::stylesheet().toUtf8())) return false;
        const auto assets = customTheme() ? customThemePath_ + "/source" : ":/site/" + themeName() + "/assets";
        QDirIterator resources(assets, QDir::Files, QDirIterator::Subdirectories);
        while (resources.hasNext()) {
            const auto path = resources.next();
            if (customTheme()) {
                const auto segments = QDir(assets).relativeFilePath(path).split('/');
                bool excluded = false;
                for (const auto &segment: segments) if (segment.startsWith('.') || segment.startsWith('_')) excluded = true;
                if (excluded) continue;
                const auto suffix = QFileInfo(path).suffix().toLower();
                if (QStringList{"styl", "stylus", "scss", "sass", "less", "ejs", "njk", "nunjucks", "pug", "swig"}.contains(suffix))
                    return fail(QCoreApplication::translate("SiteGenerator", "自定义主题资源需要预编译：") + path);
            }
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly)) return fail(QCoreApplication::translate("SiteGenerator", "无法读取站点资源：") + path);
            if (!write(path.mid(assets.size() + 1), file.readAll())) return false;
        }
        for (const auto &name: {"LICENSE", "THIRD-PARTY-NOTICES.md"}) {
            QFile file(QDir::cleanPath(assets + "/../" + name));
            if (!file.exists()) continue;
            if (!file.open(QIODevice::ReadOnly) || !write("theme-" + QString(name), file.readAll())) return false;
        }
        return true;
    }

private:
    bool fail(const QString &message) { *error_ = message; return false; }
    bool loadCustomTheme() {
        const auto configured = book_.config["themeDirectory"].toString().trimmed();
        if (configured.isEmpty()) return fail(QCoreApplication::translate("SiteGenerator", "请选择自定义主题目录"));
        customThemePath_ = QDir::cleanPath(QDir(book_.path).absoluteFilePath(configured));
        if (!QFileInfo(customThemePath_).isDir())
            return fail(QCoreApplication::translate("SiteGenerator", "找不到自定义主题目录：") + customThemePath_);
        for (const auto &name: {"layout", "index", "post", "page", "card", "archive", "category", "tag"}) {
            QFile file(customThemePath_ + "/layout/" + name + ".html");
            const bool optional = QStringList{"archive", "category", "tag"}.contains(name);
            if (optional && !file.exists()) continue;
            if (!file.open(QIODevice::ReadOnly))
                return fail(QCoreApplication::translate("SiteGenerator", "无法读取自定义主题模板（Hexo 模板需先适配为 QSNote HTML）：") + file.fileName());
            const auto source = QString::fromUtf8(file.readAll());
            if (source.trimmed().isEmpty() || source.contains("<%") || source.contains("{%"))
                return fail(QCoreApplication::translate("SiteGenerator", "自定义主题模板为空或包含不支持的模板语法：") + file.fileName());
            const QStringList variables{"root", "siteTitle", "author", "city", "githubSocial", "avatar", "avatarImage", "description", "footer", "domain",
                "title", "headTitle", "body", "permalink", "keywords", "articleStyle", "url", "date", "meta", "tags", "categories",
                "navigation", "comments", "toc", "excerpt", "pagination", "pageNumber", "pageCount", "postCount"};
            const QRegularExpression token(R"(\{\{([#/]?)([A-Za-z]+)\}\})");
            auto tokens = token.globalMatch(source);
            QString block;
            while (tokens.hasNext()) {
                const auto match = tokens.next();
                const auto prefix = match.captured(1), key = match.captured(2);
                if (!variables.contains(key) || (prefix == "#" && !block.isEmpty()) || (prefix == "/" && block != key))
                    return fail(QCoreApplication::translate("SiteGenerator", "自定义主题占位符或条件块无效：") + file.fileName() + " " + match.captured());
                if (prefix == "#") block = key;
                if (prefix == "/") block.clear();
            }
            const auto remaining = QString(source).remove(token);
            if (!block.isEmpty() || remaining.contains("{{"))
                return fail(QCoreApplication::translate("SiteGenerator", "自定义主题占位符或条件块无效：") + file.fileName());
            customTemplates_[name] = source;
        }
        return true;
    }
    bool validPath(const QString &path) {
        const auto decoded = QUrl::fromPercentEncoding(path.toUtf8());
        if (path.startsWith('/') || decoded.contains("//") || decoded.contains('\\') || decoded.contains(':') || decoded.contains('?') || decoded.contains('#') || decoded.contains(QChar::Null))
            return fail(QCoreApplication::translate("SiteGenerator", "无效的站点路径：") + path);
        for (const auto &part: decoded.split('/')) if (part == "." || part == ".." || part.startsWith('.')) return fail(QCoreApplication::translate("SiteGenerator", "无效的站点路径：") + path);
        return true;
    }
    bool write(const QString& path, const QByteArray &data) {
        if (!validPath(path)) return false;
        if (outputs_.contains(path)) return fail(QCoreApplication::translate("SiteGenerator", "站点输出路径冲突：") + path);
        outputs_.insert(path);
        const auto target = output_ + "/" + path;
        if (!QDir().mkpath(QFileInfo(target).absolutePath())) return fail(QCoreApplication::translate("SiteGenerator", "无法创建目录：") + target);
        if (QFileInfo::exists(target)) return fail(QCoreApplication::translate("SiteGenerator", "站点输出路径冲突：") + path);
        QFile file(target);
        if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size()) return fail(target + ": " + file.errorString());
        return true;
    }
    [[nodiscard]] QString href(const QString &route) const { return urlPath(root_ + route); }
    bool render(Page &page) {
        QDomDocument document;
        if (!renderer_.render(page.body, document, error_)) {
            *error_ = page.title + "：" + *error_;
            return false;
        }
        page.text = document.documentElement().text();
        struct Heading { int level; QString markup; bool children = false; };
        QList<Heading> toc;
        const auto closeHeading = [&] {
            const auto entry = toc.takeLast();
            const auto markup = entry.markup + (entry.children ? "</ol>" : "") + "</li>";
            if (toc.isEmpty()) page.toc += markup;
            else {
                if (!toc.last().children) toc.last().markup += R"(<ol class="toc-child">)";
                toc.last().children = true;
                toc.last().markup += markup;
            }
        };
        const auto elements = ArticleRenderer::elements(document);
        for (auto node: elements) {
            const auto tag = node.tagName();
            if (QRegularExpression("^h[1-6]$").match(tag).hasMatch()) {
                const auto title = node.text();
                const auto id = node.attribute("id");
                const auto level = tag.mid(1).toInt();
                while (!toc.isEmpty() && toc.last().level >= level) closeHeading();
                toc.append({level, "<li class=\"toc-item toc-level-" + QString::number(level) + R"("><a class="toc-link" href="#)" + escaped(urlPath(id)) + R"("><span class="toc-text">)" + escaped(title) + "</span></a>"});
            }
            if (tag != "a" && tag != "img") continue;
            const auto attribute = tag == "img" ? "src" : "href";
            const QUrl original(node.attribute(attribute));
            if (tag == "img" && node.attribute(attribute).startsWith("data:image/png;base64,")) {
                const auto encoded = original.toString(QUrl::FullyEncoded).toLatin1();
                const auto comma = encoded.indexOf(',');
                const auto bytes = QByteArray::fromBase64(encoded.mid(comma + 1));
                const auto path = "assets/qsnote/" + QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()) + ".png";
                if (!outputs_.contains(path) && !write(path, bytes)) return false;
                node.setAttribute(attribute, href(path));
                continue;
            }
            if (!original.isRelative() || original.path().isEmpty()) continue;
            QString target;
            if (original.path().startsWith('/')) target = href(original.path().mid(1));
            else {
                const auto path = QDir::cleanPath(QFileInfo(page.source).dir().absoluteFilePath(original.path()));
                if (links_.contains(path)) target = href(links_[path]);
            }
            if (target.isEmpty()) continue;
            if (original.hasQuery()) target += '?' + original.query(QUrl::FullyEncoded);
            if (original.hasFragment()) target += '#' + original.fragment(QUrl::FullyEncoded);
            node.setAttribute(attribute, target);
        }
        while (!toc.isEmpty()) closeHeading();
        page.html = ArticleRenderer::html(document);
        return true;
    }
    [[nodiscard]] bool nextTheme() const { return book_.config["theme"].toString() == "next"; }
    [[nodiscard]] bool yiliaTheme() const { return book_.config["theme"].toString() == "yilia"; }
    [[nodiscard]] bool threeTheme() const { return book_.config["theme"].toString() == "3-jekyll"; }
    [[nodiscard]] bool customTheme() const { return book_.config["theme"].toString() == "custom"; }
    [[nodiscard]] QString themeName() const { return nextTheme() ? "next" : yiliaTheme() ? "yilia" : threeTheme() ? "3-jekyll" : "pure"; }
    [[nodiscard]] QString themeTemplate(const QString &name, QMap<QString, QString> values = {}) const {
        QString source;
        if (customTheme()) source = customTemplates_.value(name);
        else {
            QFile file(":/site/" + themeName() + "/templates/" + name + ".html");
            if (!file.open(QIODevice::ReadOnly)) return {};
            source = QString::fromUtf8(file.readAll());
        }
        values["root"] = escaped(href({}));
        values["siteTitle"] = escaped(book_.config["title"].toString(book_.name));
        values["author"] = escaped(book_.config["author"].toString());
        values["city"] = escaped(book_.config["city"].toString().trimmed());
        const auto github = book_.config["github"].toString().trimmed();
        const QUrl githubUrl(github);
        values["githubSocial"] = githubUrl.scheme() == "https" || githubUrl.scheme() == "http" ? escaped(github) : QString();
        auto avatar = book_.config["avatar"].toString().trimmed();
        if (avatar.isEmpty() && !yiliaTheme() && !customTheme())
            avatar = nextTheme() ? "images/avatar.gif" : threeTheme() ? "assets/img/avatar.jpg" : "images/avatar.jpg";
        const QUrl avatarUrl(avatar);
        if (!avatar.isEmpty() && avatarUrl.isRelative() && !avatar.startsWith('/'))
            avatar = href(avatarUrl.path(QUrl::FullyDecoded))
                + (avatarUrl.hasQuery() ? "?" + avatarUrl.query(QUrl::FullyEncoded) : QString())
                + (avatarUrl.hasFragment() ? "#" + avatarUrl.fragment(QUrl::FullyEncoded) : QString());
        values["avatar"] = escaped(avatar);
        values["avatarImage"] = avatar.isEmpty() ? QString() : R"(<img class="show" src=")" + escaped(avatar)
            + "\" alt=\"" + values["author"] + R"(" width="128" height="128">)";
        values["description"] = escaped(book_.config["description"].toString());
        values["footer"] = escaped(book_.config["footer"].toString("Powered by QSNote"));
        values["domain"] = escaped(book_.config["domain"].toString(href({})));
        // Optional, non-nested blocks are evaluated before inserting user values.
        QString conditionalSource;
        qsizetype blockOffset = 0;
        auto blocks = QRegularExpression(R"(\{\{#([A-Za-z]+)\}\}([\s\S]*?)\{\{/\1\}\})").globalMatch(source);
        while (blocks.hasNext()) {
            const auto block = blocks.next();
            conditionalSource += source.mid(blockOffset, block.capturedStart() - blockOffset);
            if (!values.value(block.captured(1)).isEmpty()) conditionalSource += block.captured(2);
            blockOffset = block.capturedEnd();
        }
        source = conditionalSource + source.mid(blockOffset);
        QString result;
        qsizetype offset = 0;
        auto matches = QRegularExpression(R"(\{\{([A-Za-z]+)\}\})").globalMatch(source);
        while (matches.hasNext()) {
            const auto match = matches.next();
            result += source.mid(offset, match.capturedStart() - offset) + values.value(match.captured(1));
            offset = match.capturedEnd();
        }
        return result + source.mid(offset);
    }
    [[nodiscard]] QString link(const QString &route, const QString &title, const QString &classes = {}) const {
        return "<a class=\"" + classes + "\" href=\"" + escaped(href(route)) + "\">" + escaped(title) + "</a>";
    }
    [[nodiscard]] QString taxonomyLinks(const Page &page, bool tags = false) const {
        QString result;
        if (yiliaTheme()) {
            for (const auto &name: tags ? page.tags : page.categories)
                result += R"(<li class="article-tag-list-item">)" + link((tags ? "tags/" : "categories/") + name + "/", name,
                    "article-tag-list-link color" + QString::number(name.size() % 5 + 1)) + "</li>";
            if (result.isEmpty()) return {};
            return "<div class=\"" + QString(tags ? "article-tag" : "article-category") + " tagcloud\"><i class=\"icon " +
                (tags ? "icon-price-tags" : "icon-book") + R"("></i><ul class="article-tag-list">)" + result + "</ul></div>";
        }
        for (const auto &name: tags ? page.tags : page.categories) {
            if (nextTheme() && tags) result += link("tags/" + name + "/", "# " + name);
            else result += link((tags ? "tags/" : "categories/") + name + "/", name, tags ? "article-tag-link-link" : "article-category-link");
        }
        return result;
    }
    [[nodiscard]] QString meta(const Page &page, bool card = false) const {
        const auto date = escaped(page.date.left(10));
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
    [[nodiscard]] QString permalink(const QString &route) const {
        const QUrl domain(book_.config["domain"].toString());
        if (domain.host().isEmpty()) return href(route);
        return domain.scheme() + "://" + domain.authority() + href(route);
    }
    [[nodiscard]] QString article(const Page &page, bool index = false) const {
        if (customTheme()) return themeTemplate("post", {{"title", escaped(page.title)}, {"body", page.html},
            {"url", escaped(href(page.route))}, {"permalink", escaped(permalink(page.route))}, {"date", escaped(page.date.left(10))},
            {"meta", meta(page)}, {"tags", taxonomyLinks(page, true)}, {"categories", taxonomyLinks(page)},
            {"navigation", postNavigation(page)}, {"comments", comments(book_, page)}, {"toc", page.toc.isEmpty() ? QString() : "<ol>" + page.toc + "</ol>"}});
        if (!nextTheme() && !yiliaTheme() && !threeTheme()) {
            auto result = themeTemplate("post", {{"articleId", escaped("post-" + page.route)}, {"title", escaped(page.title)}, {"meta", meta(page)}, {"body", page.html},
                {"url", escaped(href(page.route))}, {"permalink", escaped(permalink(page.route))}, {"comments", index ? QString() : comments(book_, page)}});
            if (index) result.replace(R"(<h1 class="article-title" itemprop="name">)" + escaped(page.title) + "</h1>", "<h1 itemprop=\"name\">" + link(page.route, page.title, "article-title") + "</h1>");
            return result;
        }
        return themeTemplate("post", {{"title", escaped(page.title)}, {"meta", meta(page)}, {"body", page.html},
            {"tags", taxonomyLinks(page, true)}, {"categories", taxonomyLinks(page)}, {"navigation", postNavigation(page)}, {"comments", comments(book_, page)}});
    }
    [[nodiscard]] QString postNavigation(const Page &page) const {
        const auto index = allPosts_.indexOf(&page);
        const auto *previous = index > 0 ? allPosts_[index - 1] : nullptr;
        const auto *next = index + 1 < allPosts_.size() ? allPosts_[index + 1] : nullptr;
        if (customTheme()) return (previous ? link(previous->route, "‹ " + previous->title, "prev") : QString())
            + (next ? link(next->route, next->title + " ›", "next") : QString());
        if ((nextTheme() || yiliaTheme() || threeTheme()) && !previous && !next) return {};
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
        return result + "</ul><div class=\"bar-right\"></div></div></nav>";
    }
    [[nodiscard]] QString collection(const QList<const Page *> &posts) const {
        QString result;
        for (const auto *post: posts) result += R"(<a class="collection-item" itemprop="url" href=")" + escaped(href(post->route)) + "\"><time datetime=\"" + escaped(post->date) + R"(" itemprop="datePublished">)" + escaped(post->date.left(10)) + "</time> <span>   </span> " + escaped(post->title) + "</a>";
        return result;
    }
    [[nodiscard]] QString taxonomy(const QString &kind, const QString &selected = {}, const QList<const Page *> &subset = {}) const {
        const auto &groups = kind == "categories" ? categories_ : tags_;
        const auto title = kind == "categories" ? "分类" : "标签";
        if (yiliaTheme() || threeTheme() || customTheme()) {
            QString terms;
            for (auto it = groups.cbegin(); it != groups.cend(); ++it)
                terms += "<li>" + link(kind + "/" + it.key() + "/", it.key()) + " (" + QString::number(it.value().size()) + ")</li>";
            return themeTemplate("page", {{"title", title}, {"body", "<ul>" + terms + "</ul>"}});
        }
        QString body, nav = "<li>" + link(kind + "/", "All") + "</li>";
        for (auto it = groups.cbegin(); it != groups.cend(); ++it) {
            const auto url = kind + "/" + it.key() + "/";
            nav += "<li>" + link(url, it.key()) + "</li>";
            if (!selected.isEmpty() && it.key() != selected) continue;
            const auto &posts = selected.isEmpty() ? it.value() : subset;
            if (nextTheme()) {
                body += kind == "categories" ? R"(<li class="category-list-item">)" + link(url, it.key(), "category-list-link") +
                    R"(<span class="category-list-count">)" + QString::number(posts.size()) + "</span></li>"
                    : link(url, it.key()) + " ";
            } else if (kind == "categories") {
                const auto id = "collapse" + escaped(it.key());
                body += R"(<div class="panel panel-default b-no"><div class="panel-heading" role="tab"><h3 class="panel-title"><a data-toggle="collapse" href="#)" + id + R"(" aria-expanded="true"><i class="icon icon-folder text-active"></i><i class="icon icon-folder-open text"></i> )" + escaped(it.key()) + "</a> <small class=\"text-muted\">(Total " + QString::number(posts.size()) + " articles)</small></h3></div><div id=\"" + id + R"(" class="panel-collapse collapse in" role="tabpanel"><div class="panel-body"><div class="collection">)" + collection(posts) + "</div></div></div></div>";
            } else {
                body += "<h3 class=\"panel-title mb-1x\">" + link(url, "# " + it.key()) + " <small class=\"text-muted\">(Total " + QString::number(posts.size()) + " articles)</small></h3><div class=\"row\">";
                for (const auto *post: posts) body += R"(<div class="col-md-6"><article class="panel panel-default hover-shadow hover-grow" itemscope itemtype="http://schema.org/BlogPosting"><div class="panel-body"><div class="article-meta"><time>)" + escaped(post->date.left(10)) + R"(</time></div><h3 class="article-title" itemprop="name">)" + link(post->route, post->title, "article-link") + "</h3></div><div class=\"panel-footer\">" + tagLabels(*post) + "</div></article></div>";
                body += "</div>";
            }
        }
        if (nextTheme()) return themeTemplate("page", {{"title", title}, {"body", kind == "categories" ?
            R"(<ul class="category-list">)" + body + "</ul>" : R"(<div class="tag-cloud"><div class="tag-cloud-tags">)" + body + "</div></div>"}});
        return "<article class=\"article article-" + kind + R"( article-type-list"><header class="article-header"><h1 class="hidden-xs" itemprop="name">)" + title + (selected.isEmpty() ? QString() : ": " + escaped(selected)) + "</h1><p class=\"text-muted hidden-xs\">共 " + QString::number(selected.isEmpty() ? groups.size() : subset.size()) + (selected.isEmpty() ? " 个" + QString(title) : " 篇文章") + R"(</p><nav role="navigation" id="nav-main" class="okayNav"><ul>)" + nav + "</ul></nav></header><div class=\"article-body\">" + body + "</div></article>";
    }
    [[nodiscard]] QString tagLabels(const Page &page) const {
        QString result;
        for (const auto &tag: page.tags.mid(0, 3)) result += link("tags/" + tag + "/", tag, "label label-default mb") + " ";
        return result;
    }
    [[nodiscard]] QString sidebarRecent() const {
        QString result;
        auto recent = allPosts_;
        for (const auto *page: recent.mid(0, 5)) {
            result += R"(<li><div class="item-inner"><p class="item-category">)" + taxonomyLinks(*page).replace("article-category-link", "category-link") + "</p><p class=\"item-title\">" + link(page->route, page->title, "title") + R"(</p><p class="item-date"><time datetime=")" + escaped(page->date) + R"(" itemprop="datePublished">)" + escaped(page->date.left(10)) + "</time></p></div></li>";
        }
        return result;
    }
    [[nodiscard]] QString sidebarTerms(const QMap<QString, QList<const Page *>> &groups, const QString &type, bool cloud = false) const {
        QString result;
        for (auto it = groups.cbegin(); it != groups.cend(); ++it) {
            const auto url = (type == "category" ? "categories" : type + "s") + "/" + it.key() + "/";
            if (cloud) result += "<a href=\"" + escaped(href(url)) + R"(" style="font-size: 13px;">)" + escaped(it.key()) + "</a> ";
            else result += "<li class=\"" + type + "-list-item\">" + link(url, it.key(), type + "-list-link") + "<span class=\"" + type + "-list-count\">" + QString::number(it.value().size()) + "</span></li>";
        }
        return result;
    }
    bool html(QString route, const QString &title, const QString &body, const Page *page = nullptr) {
        auto keywords = page && !page->keywords.isEmpty() ? page->keywords : book_.config["keywords"].toString().trimmed();
        keywords.replace(QChar(0xff0c), QChar(','));
        if (customTheme()) {
            const auto markup = themeTemplate("layout", {{"headTitle", escaped(title + " | " + book_.config["title"].toString(book_.name))},
                {"title", escaped(title)}, {"body", body}, {"permalink", escaped(permalink(route))}, {"keywords", escaped(keywords)},
                {"articleStyle", R"(<link rel="stylesheet" href=")" + escaped(href("css/qsnote-article.css")) + "\">"}});
            if (route.isEmpty() || route.endsWith('/')) route += "index.html";
            return write(route, markup.toUtf8());
        }
        QMap<QString, QString> values{{"headTitle", escaped(title + " | " + book_.config["title"].toString(book_.name))}, {"content", body},
            {"keywordsMeta", keywords.isEmpty() ? QString() : R"(<meta name="keywords" content=")" + escaped(keywords) + "\">"},
            {"title", escaped(title)}, {"permalink", escaped(permalink(route))}, {"sidebarRecent", sidebarRecent()},
            {"sidebarCategories", sidebarTerms(categories_, "category")}, {"sidebarTags", sidebarTerms(tags_, "tag")}, {"sidebarCloud", sidebarTerms(tags_, "tag", true)},
            {"layout", route.startsWith("archives/") || (!page && (route.startsWith("categories/") || route.startsWith("tags/"))) ? "archive posts-collapse" : page ? (page->post ? "post posts-expand" : "page posts-expand") : "index posts-expand"},
            {"sidebarClass", page && page->post && !page->toc.isEmpty() ? "sidebar-nav-active sidebar-toc-active" : "sidebar-overview-active"},
            {"toc", page && page->post && !page->toc.isEmpty() ? R"(<div class="post-toc animated"><ol class="nav">)" + QString(page->toc).replace("class=\"toc-", "class=\"nav-") + "</ol></div>" : QString()},
            {"postCount", QString::number(allPosts_.size())}, {"categoryCount", QString::number(categories_.size())}, {"tagCount", QString::number(tags_.size())},
            {"config", json({{"root", href({})}, {"path", href("search.json")}, {"scheme", "Muse"},
                {"sidebar", QJsonObject{{"position", "left"}, {"display", "post"}}}, {"motion", QJsonObject{{"enable", false}}},
                {"codeblock", QJsonObject{{"fold", QJsonObject{{"enable", false}}}, {"copy_button", QJsonObject{{"enable", false}}}}},
                {"comments", QJsonObject{{"lazyload", false}}}, {"localsearch", QJsonObject{{"preload", true}, {"top_n_per_article", 1}, {"unescape", false}}},
                {"i18n", QJsonObject{{"hits", "找到 ${hits} 个搜索结果"}}}})},
            {"pageConfig", json({{"isHome", route.isEmpty()}, {"isPost", page && page->post}, {"lang", "zh-CN"}, {"sidebar", ""}})}};
        const QStringList months{"一月", "二月", "三月", "四月", "五月", "六月", "七月", "八月", "九月", "十月", "十一月", "十二月"};
        QString archives;
        for (auto it = archives_.cend(); it != archives_.cbegin();) {
            --it;
            if (it.key().size() != 8) continue;
            archives += "<li class=\"archive-list-item\">" + link("archives/" + it.key(), months.value(it.key().mid(5, 2).toInt() - 1) + " " + it.key().left(4), "archive-list-link") + "<span class=\"archive-list-count\">" + QString::number(it.value().size()) + "</span></li>";
        }
        values["sidebarArchives"] = archives;
        if (threeTheme()) {
            QString filters, entries, menu;
            for (auto it = tags_.cbegin(); it != tags_.cend(); ++it)
                filters += R"(<li><button class="sidebar-tag" type="button" aria-pressed="false" data-filter=")" + escaped(it.key()) + "\">" + escaped(it.key()) + "</button></li>";
            for (const auto *post: allPosts_) {
                const auto tags = QString::fromUtf8(QJsonDocument(QJsonArray::fromStringList(post->tags)).toJson(QJsonDocument::Compact));
                entries += R"(<a class="toc-link" data-tags=")" + escaped(tags) + "\" href=\"" + escaped(href(post->route)) + "\"" +
                    (page == post ? QString(" aria-current=\"page\"") : QString()) + " title=\"" + escaped(post->title) + "\">" + escaped(post->title) + "</a>";
            }
            for (const auto &item: QList<QPair<QString, QString>>{{"", "首页"}, {"categories/", "分类"}, {"tags/", "标签"},
                    {"archives/", "归档"}, {"links/", "友链"}, {"about/", "关于"}})
                menu += link(item.first, item.second);
            values["filters"] = filters;
            values["entries"] = entries;
            values["menu"] = menu;
        } else if (yiliaTheme()) {
            QString menu;
            for (const auto &item: QList<QPair<QString, QString>>{{"", "主页"}, {"categories/", "分类"}, {"tags/", "标签"},
                    {"archives/", "归档"}, {"links/", "友链"}, {"about/", "关于"}})
                menu += "<li>" + link(item.first, item.second) + "</li>";
            values["menu"] = menu;
            values["toc"] = page && page->post && !page->toc.isEmpty() ?
                R"(<div class="toc-container tooltip-left" tabindex="0" aria-label="文章目录"><i class="icon-font icon-category"></i><div class="tooltip tooltip-east"><span class="tooltip-item"></span><div class="tooltip-content"><div class="toc-article"><ol class="toc">)" + page->toc + "</ol></div></div></div></div>" : QString();
        } else if (!nextTheme()) {
            values["noticeWidget"] = route == "about/" ? QString() : themeTemplate("noticeWidget");
            values["categoryWidget"] = route == "about/" || categories_.isEmpty() ? QString() : themeTemplate("categoryWidget", values);
            values["tagWidget"] = route == "about/" || tags_.isEmpty() ? QString() : themeTemplate("tagWidget", values);
            values["cloudWidget"] = route == "about/" || tags_.isEmpty() ? QString() : themeTemplate("cloudWidget", values);
            values["archiveWidget"] = route == "about/" || allPosts_.isEmpty() ? QString() : themeTemplate("archiveWidget", values);
            values["recentWidget"] = route == "about/" || allPosts_.isEmpty() ? QString() : themeTemplate("recentWidget", values);
        }
        auto markup = themeTemplate("shell", values);
        markup.replace("</head>", R"(<link rel="stylesheet" href=")" + escaped(href("css/qsnote-article.css")) + "\"></head>");
        if (route.isEmpty() || route.endsWith('/')) route += "index.html";
        return write(route, markup.toUtf8());
    }
    [[nodiscard]] QString pagination(const QString &route, int page, int count) const {
        if (nextTheme() || yiliaTheme() || threeTheme() || customTheme()) {
            if (count == 1) return {};
            QString result = yiliaTheme() ? R"(<nav id="page-nav">)" : R"(<nav class="pagination">)";
            if (page > 1) result += link(route + (page == 2 ? QString() : "page/" + QString::number(page - 1) + "/"), "‹", "extend prev");
            for (int i = 1; i <= count; ++i) result += i == page ? "<span class=\"page-number current\">" + QString::number(i) + "</span>" : link(route + (i == 1 ? QString() : "page/" + QString::number(i) + "/"), QString::number(i), "page-number");
            if (page < count) result += link(route + "page/" + QString::number(page + 1) + "/", "›", "extend next");
            return result + "</nav>";
        }
        return R"(<nav class="bar bar-footer clearfix" data-stick-bottom><div class="bar-inner"><ul class="pager pull-left"><li class="prev)" + QString(page == 1 ? " disabled" : "") + "\"><a href=\"" + (page == 1 ? "javascript:;" : escaped(href(route + (page == 2 ? QString() : "page/" + QString::number(page - 1) + "/")))) + "\"><i class=\"icon icon-angle-left\"></i> 上一页</a></li><li class=\"next" + QString(page == count ? " disabled" : "") + "\"><a href=\"" + (page == count ? "javascript:;" : escaped(href(route + "page/" + QString::number(page + 1) + "/"))) + "\">下一页 <i class=\"icon icon-angle-right\"></i></a></li></ul><div class=\"total-article bar-right\">Page " + QString::number(page) + " of " + QString::number(count) + "</div></div></nav>";
    }
    bool listing(const QString &route, const QString &title, const QList<const Page *> &posts) {
        constexpr int pageSize = 10;
        const auto count = qMax(1, int((posts.size() + pageSize - 1) / pageSize));
        for (int page = 1; page <= count; ++page) {
            const auto subset = posts.mid((page - 1) * pageSize, pageSize);
            QString body;
            if (customTheme()) {
                QString entries;
                for (const auto *post: subset) entries += themeTemplate("card", {{"title", escaped(post->title)}, {"url", escaped(href(post->route))},
                    {"meta", meta(*post, true)}, {"date", escaped(post->date.left(10))}, {"tags", taxonomyLinks(*post, true)},
                    {"categories", taxonomyLinks(*post)}, {"excerpt", escaped(post->text.size() > 128 ? post->text.left(125) + "..." : post->text)}});
                auto name = route.startsWith("categories/") ? QString("category") : route.startsWith("tags/") ? QString("tag")
                    : route.startsWith("archives/") ? QString("archive") : QString("index");
                if (!customTemplates_.contains(name)) name = name == "category" || name == "tag" ? "archive" : "index";
                if (!customTemplates_.contains(name)) name = "index";
                body = themeTemplate(name, {{"title", escaped(title)}, {"body", entries}, {"pagination", pagination(route, page, count)},
                    {"pageNumber", QString::number(page)}, {"pageCount", QString::number(count)}, {"postCount", QString::number(posts.size())}});
                if (!html(route + (page == 1 ? QString() : "page/" + QString::number(page) + "/"), title, body)) return false;
                continue;
            }
            if (threeTheme()) {
                QString entries;
                for (const auto *post: subset) entries += themeTemplate("card", {{"title", escaped(post->title)}, {"url", escaped(href(post->route))},
                    {"meta", meta(*post, true)}, {"excerpt", escaped(post->text.size() > 128 ? post->text.left(125) + "..." : post->text)}});
                body = themeTemplate("page", {{"title", escaped(title)}, {"body", entries.isEmpty() ? QString("<p>暂无文章</p>") : entries}});
            } else if (route.startsWith("archives/") || ((nextTheme() || yiliaTheme()) && (route.startsWith("categories/") || route.startsWith("tags/")))) {
                QString sections;
                QMap<QString, QList<const Page *>> years;
                for (const auto *post: subset) years[post->date.left(4)].append(post);
                for (auto it = years.cend(); it != years.cbegin();) {
                    --it;
                    if (yiliaTheme()) {
                        sections += R"(<section class="archives-wrap"><div class="archive-year-wrap">)" + link("archives/" + it.key() + "/", it.key(), "archive-year") + R"(</div><div class="archives">)";
                        for (const auto *post: it.value()) sections += themeTemplate("archive", {{"title", escaped(post->title)}, {"url", escaped(href(post->route))},
                            {"meta", meta(*post, true)}, {"tags", taxonomyLinks(*post, true)}, {"categories", taxonomyLinks(*post)}});
                        sections += "</div></section>";
                    } else if (nextTheme()) {
                        sections += R"(<div class="collection-year"><span class="collection-header">)" + escaped(it.key()) + "</span></div>";
                        for (const auto *post: it.value()) sections += R"(<article><header class="post-header"><div class="post-meta-container"><time>)" + escaped(post->date.mid(5, 5)) +
                            R"(</time></div><div class="post-title">)" + link(post->route, post->title, "post-title-link") + "</div></header></article>";
                    } else sections += R"(<section class="panel panel-default b-no"><div class="panel-heading" role="tab"><h3 class="panel-title"><a data-toggle="collapse" href="#collapse)" + escaped(it.key()) + R"(" aria-expanded="true"><i class="icon icon-calendar-plus text-active"></i><i class="icon icon-calendar-minus text"></i> )" + escaped(it.key()) + R"(</a></h3></div><div class="panel-collapse collapse in" id="collapse)" + escaped(it.key()) + R"("><div class="panel-body"><div class="collection">)" + collection(it.value()) + "</div></div></div></section>";
                }
                if (yiliaTheme()) body = sections.isEmpty() ? themeTemplate("page", {{"title", escaped(title)}, {"body", "暂无文章"}}) : sections;
                else body = nextTheme() ? R"(<div class="post-block"><div class="post-content"><div class="collection-title"><span class="collection-header">)" + escaped(title) + " · 共 " + QString::number(posts.size()) + " 篇文章</span></div>" + sections + "</div></div>" : R"(<article class="content article article-archives article-type-list"><header class="article-header"><h1 itemprop="title">)" + escaped(title) + "</h1><p class=\"text-muted\">共 " + QString::number(posts.size()) + " 篇文章</p></header><div class=\"article-body\">" + sections + "</div></article>";
            } else if (!nextTheme() && !yiliaTheme() && (route.startsWith("categories/") || route.startsWith("tags/"))) body = taxonomy(route.section('/', 0, 0), title, subset);
            else if (!nextTheme() && !yiliaTheme() && subset.size() == 1) body = article(*subset.first(), true);
            else {
                body = nextTheme() || yiliaTheme() ? QString() : "<div class=\"content article-list\">";
                for (const auto *post: subset) body += themeTemplate("card", {{"title", escaped(post->title)}, {"url", escaped(href(post->route))}, {"meta", meta(*post, true)},
                    {"tags", taxonomyLinks(*post, true)}, {"categories", taxonomyLinks(*post)}, {"excerpt", escaped(post->text.size() > 128 ? post->text.left(125) + "..." : post->text)}});
                if (!nextTheme() && !yiliaTheme()) body += "</div>";
            }
            if (!html(route + (page == 1 ? QString() : "page/" + QString::number(page) + "/"), title, body + pagination(route, page, count))) return false;
        }
        return true;
    }
    const Notebook &book_;
    QString output_;
    QString *error_;
    ArticleRenderer renderer_;
    QString root_;
    QString customThemePath_;
    QMap<QString, QString> customTemplates_;
    QList<Page> pages_;
    QMap<QString, QString> links_;
    QList<const Page *> allPosts_;
    QMap<QString, QList<const Page *>> categories_, tags_, archives_;
    QSet<QString> outputs_;
};
}

bool StaticSite::generate(const Notebook &book, QString *error) {
    QTemporaryDir staging(book.path + "/.qsnote-site-XXXXXX");
    if (!staging.isValid()) {
        *error = QCoreApplication::translate("SiteGenerator", "无法创建站点生成临时目录");
        return false;
    }
    const auto output = staging.path() + "/public";
    if (!Generator(book, output, error).generate()) return false;
    const auto destination = book.path + "/public";
    const auto backup = staging.path() + "/previous";
    const bool existing = QFileInfo::exists(destination);
    if (QFileInfo(destination).isSymLink() || (existing && !QDir().rename(destination, backup))) {
        *error = QCoreApplication::translate("SiteGenerator", "无法替换站点输出目录：") + destination;
        return false;
    }
    if (!QDir().rename(output, destination)) {
        const bool restored = !existing || QDir().rename(backup, destination);
        if (!restored) staging.setAutoRemove(false);
        *error = QCoreApplication::translate("SiteGenerator", "无法保存站点生成结果：") + destination + (restored ? QString() : QCoreApplication::translate("SiteGenerator", "；上一份输出保存在：") + backup);
        return false;
    }
    return true;
}
