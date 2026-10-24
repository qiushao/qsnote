#include "ArticleRenderer.h"
#include "FrontMatter.h"
#include "SiteThemeRenderer.h"
#include "StaticSite.h"
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
    Generator(const Notebook &book, QString output, QString *error)
        : book_(book), output_(std::move(output)), error_(error), renderer_(book.path, "sites"), root_(StaticSite::rootPath(book)) {}

    bool generate() {
        SiteTemplateRenderer templates(book_);
        if (!templates.load(error_)) return false;
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
            SitePage page;
            page.source = file;
            page.body = QString::fromUtf8(data);
            QJsonObject metadata;
            if (!FrontMatter::parse(page.body, metadata, error_)) {
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
        SiteIndex index;
        auto &posts = index.posts;
        auto &categories = index.categories;
        auto &tags = index.tags;
        auto &archives = index.archives;
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
        if (!listing(theme, {}, book_.config["title"].toString(book_.name), posts) || !listing(theme, "archives/", "归档", posts)) return false;
        for (auto it = archives.cbegin(); it != archives.cend(); ++it)
            if (!listing(theme, "archives/" + it.key(), it.key(), it.value())) return false;
        for (const auto &group: {qMakePair(QString("categories"), categories), qMakePair(QString("tags"), tags)})
            for (auto it = group.second.cbegin(); it != group.second.cend(); ++it)
                if (!listing(theme, group.first + "/" + it.key() + "/", it.key(), it.value())) return false;
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
        const auto assets = templates.assetsPath();
        QDirIterator resources(assets, QDir::Files, QDirIterator::Subdirectories);
        while (resources.hasNext()) {
            const auto path = resources.next();
            if (templates.theme() == SiteTemplateRenderer::Theme::Custom) {
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
            if (tag != "a" && tag != "img") continue;
            const auto attribute = tag == "img" ? "src" : "href";
            const auto value = node.attribute(attribute);
            if (tag == "img" && value.startsWith("data:image/png;base64,")) {
                const auto bytes = QByteArray::fromBase64(value.mid(value.indexOf(',') + 1).toLatin1());
                const auto path = "assets/qsnote/" + QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()) + ".png";
                if (!outputs_.contains(path) && !write(path, bytes)) return false;
                node.setAttribute(attribute, href(path));
                continue;
            }
            const QUrl original(value);
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
