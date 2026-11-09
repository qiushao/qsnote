#include "ArticleRenderer.h"
#include "FrontMatter.h"
#include "NotebookCategories.h"
#include "SiteThemeRenderer.h"
#include "StaticSite.h"
#include "SiteDocuments.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <QTemporaryDir>
#include <QUrl>
#include <algorithm>
#include <utility>

namespace {
QString escaped(const QString &text) { return text.toHtmlEscaped(); }
QString urlPath(const QString &path) { return QString::fromLatin1(QUrl::toPercentEncoding(path, "/")); }
QStringList strings(const QJsonValue &value) {
    QStringList result;
    if (value.isString()) result.append(value.toString());
    else for (const auto &entry: value.toArray()) if (entry.isString()) result.append(entry.toString());
    result.removeAll(QString());
    result.removeDuplicates();
    return result;
}

class Generator {
public:
    Generator(const Notebook &book, QString output, QString *error, const StaticSite::ProgressCallback &progress)
        : book_(book), output_(std::move(output)), error_(error), renderer_(book.path, "sites"), root_(StaticSite::rootPath(book)), progress_(progress) {}

    bool generate() {
        if (!SiteDocuments::validate(book_, error_) || !SiteDocuments::validateNavigation(book_, error_)) return false;
        report(QCoreApplication::translate("SiteGenerator", "正在加载站点主题…"));
        SiteTemplateRenderer templates(book_);
        if (!templates.load(error_)) return false;
        stage_ = 1;
        report(QCoreApplication::translate("SiteGenerator", "正在扫描站点源文件…"));
        const auto source = book_.path + "/source";
        if (!QFileInfo(source).isDir()) return fail(QCoreApplication::translate("SiteGenerator", "找不到站点 source 目录"));
        QDirIterator files(source, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
        QStringList sources;
        while (files.hasNext()) sources.append(files.next());
        sources.sort();
        QSet<QString> documentPaths;
        QMap<QString, QString> documentDirectories;
        for (const auto &note: book_.notes) {
            if (!SiteDocuments::isDocument(note)) continue;
            auto path = note.path.mid(7);
            if (!path.startsWith("docs/")) continue;
            const auto group = SiteDocuments::collection(book_, note.metadata["collectionId"].toString());
            documentDirectories.insert(path.section('/', 0, 1), "docs/" + group["slug"].toString() + '/');
            if (note.folder) documentDirectories.insert(path, SiteDocuments::route(book_, note));
            while (!path.isEmpty()) {
                documentPaths.insert(path);
                const auto separator = path.lastIndexOf('/');
                if (separator < 0) break;
                path.truncate(separator);
            }
        }
        int sourceIndex = 0;
        for (const auto &file: sources) {
            const auto relative = QDir(source).relativeFilePath(file);
            report(QCoreApplication::translate("SiteGenerator", "正在读取源文件（%1/%2）：%3").arg(++sourceIndex).arg(sources.size()).arg(relative));
            const auto segments = relative.split('/');
            bool excluded = false;
            QString path;
            for (const auto &segment: segments) {
                path += (path.isEmpty() ? QString() : QString("/")) + segment;
                if ((segment.startsWith('.') || (segment.startsWith('_') && segment != "_posts")) && !documentPaths.contains(path)) excluded = true;
            }
            if (excluded) continue;
            QFile input(file);
            if (!input.open(QIODevice::ReadOnly)) return fail(file + ": " + input.errorString());
            const auto data = input.readAll();
            if (!file.endsWith(".md", Qt::CaseInsensitive) && !file.endsWith(".markdown", Qt::CaseInsensitive)) {
                auto target = relative.startsWith("_posts/") ? "assets/posts/" + relative.mid(7) : relative;
                qsizetype prefixLength = 0;
                for (auto it = documentDirectories.cbegin(); it != documentDirectories.cend(); ++it) {
                    if (relative.startsWith(it.key() + '/') && it.key().size() > prefixLength) {
                        prefixLength = it.key().size();
                        target = it.value() + relative.mid(prefixLength + 1);
                    }
                }
                links_[QDir::cleanPath(file)] = target;
                resources_.insert(QDir::cleanPath(file));
                if (!relative.startsWith("_posts/") && !write(target, data)) return false;
                continue;
            }
            const Note *note = nullptr;
            for (const auto &entry: book_.notes) if (!entry.folder && QDir::cleanPath(book_.path + "/" + entry.path) == QDir::cleanPath(file)) { note = &entry; break; }
            if (relative.startsWith("docs/") && (!note || !SiteDocuments::isDocument(*note))) continue;
            if (note && SiteDocuments::isDocument(*note) && note->metadata["draft"].toBool()) continue;
            SitePage page;
            if (note) {
                page.id = note->id;
                page.document = SiteDocuments::isDocument(*note);
                if (page.document) page.collectionId = note->metadata["collectionId"].toString();
            }
            page.source = file;
            TextEncoding::Format format;
            if (!TextEncoding::decode(data, page.body, format)) return fail(QCoreApplication::translate("NotebookStore", "无法解码文本文件：%1").arg(file));
            QJsonObject metadata;
            if (!FrontMatter::parse(page.body, metadata, error_)) {
                *error_ = file + ": " + *error_;
                return false;
            }
            if (metadata["draft"].toBool() || metadata["published"] == QJsonValue(false)) continue;
            page.post = relative.startsWith("_posts/");
            page.pinned = metadata["pinned"].toBool();
            page.comments = metadata["comments"] != QJsonValue(false);
            page.title = metadata["title"].toString(QFileInfo(file).completeBaseName());
            page.categories = strings(metadata["categories"]);
            if (page.post && page.categories.isEmpty()) page.categories.append("default");
            page.tags = strings(metadata["tags"]);
            page.keywords = strings(metadata["keywords"]).join(", ").trimmed();
            page.date = metadata["date"].toString(QFileInfo(file).lastModified().toString(Qt::ISODate));
            page.updated = metadata["updated"].toString(QFileInfo(file).lastModified().toString(Qt::ISODate));
            auto route = page.post ? metadata["slug"].toString().trimmed() : metadata["permalink"].toString(metadata["slug"].toString());
            if (route.isEmpty()) route = page.post ? QFileInfo(file).completeBaseName() : relative.left(relative.lastIndexOf('.'));
            if (page.post) {
                const auto category = NotebookCategories::slug(book_, page.categories.first());
                if (QUrl::fromPercentEncoding(category.toUtf8()).contains('/') || QUrl::fromPercentEncoding(route.toUtf8()).contains('/'))
                    return fail(QCoreApplication::translate("SiteGenerator", "无效的站点路径：") + category + "/" + route);
                route = "categories/" + category + "/" + route + "/";
            }
            if (page.document) route = SiteDocuments::route(book_, *note);
            if (!page.post && !page.document && (route == "index" || route.endsWith("/index"))) route.chop(5);
            if (!route.isEmpty() && !route.endsWith('/') && !route.endsWith(".html")) route += '/';
            page.route = route;
            if (!validPath(route.isEmpty() ? "index.html" : route)) return false;
            links_[QDir::cleanPath(file)] = page.route;
            pages_.append(page);
        }
        stage_ = 2;
        int pageIndex = 0;
        for (auto &page: pages_) {
            report(QCoreApplication::translate("SiteGenerator", "正在渲染页面（%1/%2）：%3").arg(++pageIndex).arg(pages_.size()).arg(page.title));
            if (!render(page)) return false;
        }
        stage_ = 3;
        report(QCoreApplication::translate("SiteGenerator", "正在生成索引和页面…"));
        SiteIndex index;
        auto &posts = index.posts;
        auto &categories = index.categories;
        auto &tags = index.tags;
        auto &archives = index.archives;
        for (const auto &page: pages_) if (!page.id.isEmpty()) index.byId.insert(page.id, &page);
        for (const auto &entry: book_.config["docCollections"].toArray()) {
            const auto group = entry.toObject();
            const auto id = group["id"].toString();
            for (const auto *note: SiteDocuments::ordered(book_, id)) {
                const auto *page = index.byId.value(note->id);
                if (page && page->document) index.documents[id].append(page);
            }
        }
        const SiteThemeRenderer theme(book_, templates, index);
        for (const auto &page: pages_) if (page.post) posts.append(&page);
        std::stable_sort(posts.begin(), posts.end(), [](const SitePage *a, const SitePage *b) { return a->date > b->date; });
        QJsonArray search;
        for (const auto *post: posts) {
            for (const auto &category: post->categories) categories[category].append(post);
            for (const auto &tag: post->tags) tags[tag].append(post);
            const auto date = QDate::fromString(post->date.left(10), Qt::ISODate);
            if (date.isValid()) {
                archives[date.toString("yyyy") + "/"].append(post);
                archives[date.toString("yyyy/MM") + "/"].append(post);
            }
            search.append(theme.searchEntry(*post));
        }
        for (auto it = categories.begin(); it != categories.end(); ++it) {
            const auto ranks = NotebookCategories::postRanks(book_, it.key());
            std::stable_sort(it.value().begin(), it.value().end(), [&ranks](const SitePage *a, const SitePage *b) {
                return ranks.value(a->id, int(ranks.size())) < ranks.value(b->id, int(ranks.size()));
            });
        }
        for (const auto &documents: index.documents) for (const auto *page: documents) search.append(theme.searchEntry(*page));
        for (const auto &entry: book_.config["docCollections"].toArray()) {
            const auto group = entry.toObject();
            SitePage page;
            page.collectionId = group["id"].toString();
            page.document = true;
            page.title = group["name"].toString();
            page.route = "docs/" + group["slug"].toString() + '/';
            page.html = "<ul>";
            for (const auto *doc: index.documents.value(page.collectionId)) page.html += "<li><a href=\"" + escaped(href(doc->route)) + "\">" + escaped(doc->title) + "</a></li>";
            page.html += "</ul>";
            if (!writeHtml(page.route, theme.renderPage(page))) return false;
        }
        auto homePosts = posts;
        std::stable_partition(homePosts.begin(), homePosts.end(), [](const SitePage *post) { return post->pinned; });
        if (!listing(theme, {}, book_.config["title"].toString(book_.name), homePosts) || !listing(theme, "archives/", "归档", posts)) return false;
        for (auto it = archives.cbegin(); it != archives.cend(); ++it)
            if (!listing(theme, "archives/" + it.key(), it.key(), it.value())) return false;
        for (const auto &group: {qMakePair(QString("categories"), categories), qMakePair(QString("tags"), tags)})
            for (auto it = group.second.cbegin(); it != group.second.cend(); ++it)
                if (!listing(theme, group.first + "/" + (group.first == "categories" ? NotebookCategories::slug(book_, it.key()) : it.key()) + "/", it.key(), it.value())) return false;
        for (const auto &page: pages_) {
            if (!writeHtml(page.route, theme.renderPage(page))) return false;
        }
        if (!write("search.json", QJsonDocument(search).toJson(QJsonDocument::Compact))) return false;
        QJsonArray insightPosts, insightPages;
        for (const auto &page: pages_) {
            (page.post ? insightPosts : insightPages).append(theme.contentEntry(page));
        }
        if (!write("content.json", QJsonDocument(QJsonObject{{"posts", insightPosts}, {"pages", insightPages}}).toJson(QJsonDocument::Compact))) return false;
        if (!write("css/qsnote-article.css", ArticleRenderer::stylesheet().toUtf8())) return false;
        stage_ = 4;
        report(QCoreApplication::translate("SiteGenerator", "正在复制主题资源…"));
        const auto assets = templates.assetsPath();
        QDirIterator resources(assets, QDir::Files, QDirIterator::Subdirectories);
        while (resources.hasNext()) {
            const auto path = resources.next();
            const auto segments = QDir(assets).relativeFilePath(path).split('/');
            bool excluded = false;
            for (const auto &segment: segments) if (segment.startsWith('.') || segment.startsWith('_')) excluded = true;
            if (excluded) continue;
            const auto suffix = QFileInfo(path).suffix().toLower();
            if (QStringList{"styl", "stylus", "scss", "sass", "less", "ejs", "njk", "nunjucks", "pug", "swig"}.contains(suffix))
                return fail(QCoreApplication::translate("SiteGenerator", "主题资源需要预编译：") + path);
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly)) return fail(QCoreApplication::translate("SiteGenerator", "无法读取站点资源：") + path);
            if (!write(path.mid(assets.size() + 1), file.readAll())) return false;
        }
        for (const auto &name: {"LICENSE", "THIRD-PARTY-NOTICES.md"}) {
            QFile file(templates.directory() + '/' + name);
            if (!file.exists()) continue;
            if (!file.open(QIODevice::ReadOnly) || !write("theme-" + QString(name), file.readAll())) return false;
        }
        QList<QJsonObject> navigation;
        for (const auto &value: book_.config["navigation"].toArray()) navigation.append(value.toObject());
        while (!navigation.isEmpty()) {
            const auto item = navigation.takeLast();
            const auto type = item["type"].toString();
            const auto target = item["target"].toString();
            if (type == "group") { for (const auto &child: item["children"].toArray()) navigation.append(child.toObject()); continue; }
            if (type == "external") continue;
            QString route;
            if (type == "note") {
                const auto *page = index.byId.value(target);
                if (!page) return fail(QCoreApplication::translate("SiteDocuments", "导航目标未发布：%1").arg(item["title"].toString()));
                route = page->route;
            } else if (type == "collection") route = "docs/" + SiteDocuments::collection(book_, target)["slug"].toString() + '/';
            else route = type == "path" ? QUrl(target).path().mid(1) : target;
            if (route.isEmpty() || route.endsWith('/')) route += "index.html";
            if (!outputs_.contains(route)) return fail(QCoreApplication::translate("SiteDocuments", "导航路径不存在：%1").arg(target));
        }
        return true;
    }

private:
    void report(const QString &message) const { if (progress_) progress_(stage_, 6, message); }
    bool fail(const QString &message) { *error_ = message; return false; }
    bool validPath(const QString &path) {
        const auto decoded = QUrl::fromPercentEncoding(path.toUtf8());
        if (path.startsWith('/') || decoded.contains("//") || decoded.contains('\\') || decoded.contains(':') || decoded.contains('?') || decoded.contains('#') || decoded.contains(QChar::Null))
            return fail(QCoreApplication::translate("SiteGenerator", "无效的站点路径：") + path);
        for (const auto &part: decoded.split('/')) if (part == "." || part == ".." || part.startsWith('.')) return fail(QCoreApplication::translate("SiteGenerator", "无效的站点路径：") + path);
        return true;
    }
    bool write(const QString& path, const QByteArray &data) {
        report(QCoreApplication::translate("SiteGenerator", "正在写入文件：%1").arg(path));
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
    bool render(SitePage &page) {
        QDomDocument document;
        if (!renderer_.render(page.body, QDir(book_.path).relativeFilePath(page.source), document, error_)) {
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
            if (tag.size() == 2 && tag[0] == 'h' && tag[1] >= '1' && tag[1] <= '6') {
                const auto title = node.text();
                const auto id = node.attribute("id");
                const auto level = tag.mid(1).toInt();
                while (!toc.isEmpty() && toc.last().level >= level) closeHeading();
                toc.append({level, "<li class=\"toc-item toc-level-" + QString::number(level) + R"("><a class="toc-link" href="#)" + escaped(urlPath(id)) + R"("><span class="toc-text">)" + escaped(title) + "</span></a>"});
            }
            for (const auto &attribute: {QString("href"), QString("src"), QString("poster"), QString("data")}) {
                if (!node.hasAttribute(attribute) || (attribute == "data" && tag != "object")) continue;
                const auto value = node.attribute(attribute);
                if (tag == "img" && attribute == "src" && value.startsWith("data:image/png;base64,")) {
                    const auto bytes = QByteArray::fromBase64(value.mid(value.indexOf(',') + 1).toLatin1());
                    const auto path = (page.post ? page.route : QString("assets/qsnote/")) + QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()) + ".png";
                    if (!outputs_.contains(path) && !write(path, bytes)) return false;
                    node.setAttribute(attribute, href(path));
                    continue;
                }
                const QUrl original(value);
                if (!original.isRelative() || original.path().isEmpty()) continue;
                const bool absolute = original.path().startsWith('/');
                const auto path = QDir::cleanPath(absolute ? book_.path + "/source" + original.path()
                                                          : QFileInfo(page.source).dir().absoluteFilePath(original.path()));
                QString target;
                if (resources_.contains(path)) {
                    auto route = links_[path];
                    if (page.post || !outputs_.contains(route)) {
                        QFile file(path);
                        if (!file.open(QIODevice::ReadOnly)) return fail(path + ": " + file.errorString());
                        const auto bytes = file.readAll();
                        if (page.post) route = page.route + QFileInfo(path).fileName();
                        if (outputs_.contains(route)) {
                            QFile existing(output_ + "/" + route);
                            if (!existing.open(QIODevice::ReadOnly) || existing.readAll() != bytes)
                                return fail(QCoreApplication::translate("SiteGenerator", "站点输出路径冲突：") + route);
                        } else if (!write(route, bytes)) return false;
                    }
                    target = href(route);
                } else if (links_.contains(path)) target = href(links_[path]);
                else if (absolute) target = href(original.path().mid(1));
                if (target.isEmpty()) continue;
                if (original.hasQuery()) target += '?' + original.query(QUrl::FullyEncoded);
                if (original.hasFragment()) target += '#' + original.fragment(QUrl::FullyEncoded);
                node.setAttribute(attribute, target);
            }
        }
        while (!toc.isEmpty()) closeHeading();
        page.html = ArticleRenderer::html(document);
        if (page.post && page.body.contains("<!-- more -->")) {
            QDomDocument excerpt;
            auto container = excerpt.createElement("div");
            excerpt.appendChild(container);
            bool hasExcerpt = false;
            for (auto node = document.documentElement().firstChild(); !node.isNull(); node = node.nextSibling()) {
                if (node.isComment() && node.nodeValue().trimmed() == "more") { hasExcerpt = true; break; }
                if (QRegularExpression("^h[1-6]$").match(node.nodeName()).hasMatch()) continue;
                container.appendChild(excerpt.importNode(node, true));
            }
            if (hasExcerpt) page.excerpt = ArticleRenderer::html(excerpt);
        }
        return true;
    }
    bool writeHtml(QString route, const QString &markup) {
        if (route.isEmpty() || route.endsWith('/')) route += "index.html";
        return write(route, markup.toUtf8());
    }
    bool listing(const SiteThemeRenderer &theme, const QString &route, const QString &title, const QList<const SitePage *> &posts) {
        constexpr int pageSize = 10;
        const auto count = qMax(1, int((posts.size() + pageSize - 1) / pageSize));
        for (int page = 1; page <= count; ++page) {
            const auto subset = posts.mid((page - 1) * pageSize, pageSize);
            const auto markup = theme.renderListing(route, title, posts, subset, page, count);
            if (!writeHtml(route + (page == 1 ? QString() : "page/" + QString::number(page) + "/"), markup)) return false;
        }
        return true;
    }
    const Notebook &book_;
    QString output_;
    QString *error_;
    ArticleRenderer renderer_;
    QString root_;
    QList<SitePage> pages_;
    QMap<QString, QString> links_;
    QSet<QString> outputs_;
    QSet<QString> resources_;
    const StaticSite::ProgressCallback &progress_;
    int stage_ = 0;
};
}

bool StaticSite::generate(const Notebook &book, QString *error, const ProgressCallback &progress) {
    QTemporaryDir staging(book.path + "/.qsnote-site-XXXXXX");
    if (!staging.isValid()) {
        *error = QCoreApplication::translate("SiteGenerator", "无法创建站点生成临时目录");
        return false;
    }
    const auto output = staging.path() + "/public";
    if (!Generator(book, output, error, progress).generate()) return false;
    if (progress) progress(5, 6, QCoreApplication::translate("SiteGenerator", "正在保存站点生成结果…"));
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
    if (progress) progress(6, 6, QCoreApplication::translate("SiteGenerator", "站点生成完成"));
    return true;
}
