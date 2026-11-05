#include "core/SiteThemeRenderer.h"
#include <QCoreApplication>
#include <QDebug>
#include <cstdlib>

namespace {
void check(bool condition, const char *message) {
    if (condition) return;
    qCritical() << message;
    std::exit(1);
}
}// namespace

int main(int argc, char **argv) {
    const QCoreApplication app(argc, argv);
    Notebook book;
    book.config["theme"] = "pure";
    SiteTemplateRenderer templates(book);
    SitePage post;
    post.post = true;
    post.route = "post/";
    post.title = "文章";
    post.html = R"(<h2 id="section">章节</h2><h3 id="child">子章节</h3>)";
    post.toc = R"(<li class="toc-item toc-level-2"><a class="toc-link" href="#section"><span class="toc-text">章节</span></a><ol class="toc-child"><li class="toc-item toc-level-3"><a class="toc-link" href="#child"><span class="toc-text">子章节</span></a></li></ol></li>)";
    SiteIndex index;
    index.posts = {&post};
    const SiteThemeRenderer renderer(book, templates, index);
    const auto html = renderer.renderPage(post);
    check(html.contains("class=\"toggle-toc\""), "single post is missing Catalogue toggle");
    check(html.contains("data-toggle=\"collapse\" href=\"#collapseToc\""), "Catalogue toggle is not connected to sidebar");
    check(html.contains("class=\"sidebar sidebar-toc collapse\" id=\"collapseToc\""), "Catalogue sidebar should start collapsed");
    check(html.contains("<ol class=\"toc\">" + post.toc + "</ol>"), "nested heading links are missing from sidebar");
    check(html.count("id=\"collapseToc\"") == 1, "duplicate Catalogue sidebar");

    SitePage previous = post, next = post;
    previous.route = "previous/";
    next.route = "next/";
    index.posts = {&previous, &post, &next};
    const auto navigation = renderer.renderPage(post);
    check(navigation.contains("<li class=\"prev\"><a href=\"/previous/\""), "previous post link was lost");
    check(navigation.contains("<li class=\"next\"><a href=\"/next/\""), "next post link was lost");
    check(navigation.indexOf("class=\"toggle-toc\"") > navigation.indexOf("<li class=\"next\">"), "Catalogue should follow post navigation");

    check(!renderer.renderListing("", "首页", index.posts, index.posts, 1, 1).contains("collapseToc"), "listing should not have a Catalogue");
    SitePage page = post;
    page.post = false;
    page.route = "about/";
    check(!renderer.renderPage(page).contains("collapseToc"), "standalone page should not have a post Catalogue");
    post.toc.clear();
    check(!renderer.renderPage(post).contains("collapseToc"), "post without headings should not have a Catalogue");
    qInfo() << "Pure Catalogue checks passed";
}
