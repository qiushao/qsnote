#include "core/SiteThemeRenderer.h"
#include "ui/SiteConfigDialog.h"
#include <QApplication>
#include <QComboBox>
#include <QDebug>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <cstdlib>

namespace {
void check(bool condition, const char *message) {
    if (condition) return;
    qCritical() << message;
    std::exit(1);
}

QJsonObject disqusOptions(const QString &html) {
    const auto config = html.section("var disqus_config=function(){var page=", 1, 1).section(";this.page.identifier", 0, 0);
    check(!config.contains('<') && !config.contains('&'), "Disqus settings can break out of script");
    return QJsonDocument::fromJson(config.toUtf8()).object();
}
}// namespace

int main(int argc, char **argv) {
    const QApplication app(argc, argv);
    Notebook book;
    book.type = "site";
    QWidget parent;
    parent.resize(1200, 900);
    for (const auto &platform: {"valine", "gitalk"}) {
        book.config["commentPlatform"] = platform;
        {
            SiteConfigDialog dialog(book, &parent);
            auto *select = dialog.findChild<QComboBox *>("config_commentPlatform");
            check(select && select->count() == 4, "unexpected comment platform choices");
            check(select->findData("valine") == -1 && select->findData("gitalk") == -1, "removed platforms remain selectable");
            check(select->currentData() == "none", "legacy platform should display as disabled");
            dialog.reject();
            check(book.config["commentPlatform"] == platform, "cancel changed legacy configuration");
        }
        SiteConfigDialog dialog(book, &parent);
        dialog.accept();
        check(book.config["commentPlatform"] == "none", "save did not disable legacy platform");
    }

    const QJsonObject settings{{"giscusRepo", "owner/comments"}, {"giscusRepoId", "R_test"},
                               {"giscusCategory", "Announcements"}, {"giscusCategoryId", "DIC_test"}};
    {
        SiteConfigDialog dialog(book, &parent);
        auto *select = dialog.findChild<QComboBox *>("config_commentPlatform");
        select->setCurrentIndex(select->findData("giscus"));
        for (auto it = settings.begin(); it != settings.end(); ++it) {
            auto *input = dialog.findChild<QLineEdit *>("config_" + it.key());
            check(input && !input->parentWidget()->isHidden(), "Giscus field is missing or hidden");
            input->setText(it.value().toString());
            emit input->textEdited(input->text());
        }
        auto *disqus = dialog.findChild<QLineEdit *>("config_disqus");
        check(disqus->parentWidget()->isHidden(), "Disqus field visible for Giscus");
        select->setCurrentIndex(select->findData("disqus"));
        check(!disqus->parentWidget()->isHidden(), "Disqus field hidden for Disqus");
        check(dialog.findChild<QLineEdit *>("config_giscusRepo")->parentWidget()->isHidden(), "Giscus field visible for Disqus");
        select->setCurrentIndex(select->findData("giscus"));
        dialog.accept();
    }
    book.config = QJsonDocument::fromJson(QJsonDocument(book.config).toJson()).object();
    {
        SiteConfigDialog dialog(book, &parent);
        check(dialog.findChild<QComboBox *>("config_commentPlatform")->currentData() == "giscus", "saved platform was lost");
        for (auto it = settings.begin(); it != settings.end(); ++it)
            check(dialog.findChild<QLineEdit *>("config_" + it.key())->text() == it.value().toString(), "saved Giscus field was lost");
    }

    const QJsonObject twikooSettings{{"twikooEnvId", "https://comments.example.com"}, {"twikooRegion", "ap-guangzhou"}};
    {
        SiteConfigDialog dialog(book, &parent);
        auto *select = dialog.findChild<QComboBox *>("config_commentPlatform");
        check(select->findData("twikoo") >= 0, "Twikoo option is missing");
        select->setCurrentIndex(select->findData("twikoo"));
        check(dialog.findChild<QLineEdit *>("config_giscusRepo")->parentWidget()->isHidden(), "Giscus field visible for Twikoo");
        check(dialog.findChild<QLineEdit *>("config_disqus")->parentWidget()->isHidden(), "Disqus field visible for Twikoo");
        for (auto it = twikooSettings.begin(); it != twikooSettings.end(); ++it) {
            auto *input = dialog.findChild<QLineEdit *>("config_" + it.key());
            check(input && !input->parentWidget()->isHidden(), "Twikoo field is missing or hidden");
            input->setText(it.value().toString());
            emit input->textEdited(input->text());
            select->setCurrentIndex(select->findData("none"));
            check(input->parentWidget()->isHidden(), "Twikoo field visible when comments disabled");
            select->setCurrentIndex(select->findData("twikoo"));
        }
        dialog.accept();
    }
    book.config = QJsonDocument::fromJson(QJsonDocument(book.config).toJson()).object();
    {
        SiteConfigDialog dialog(book, &parent);
        check(dialog.findChild<QComboBox *>("config_commentPlatform")->currentData() == "twikoo", "saved Twikoo platform was lost");
        for (auto it = twikooSettings.begin(); it != twikooSettings.end(); ++it)
            check(dialog.findChild<QLineEdit *>("config_" + it.key())->text() == it.value().toString(), "saved Twikoo field was lost");
    }

    {
        SiteConfigDialog dialog(book, &parent);
        auto *select = dialog.findChild<QComboBox *>("config_commentPlatform");
        auto *input = dialog.findChild<QLineEdit *>("config_disqus");
        QLabel *hint = nullptr;
        for (auto *label: dialog.findChildren<QLabel *>()) {
            if (label->text().contains("https://help.disqus.com/en/articles/1717112-universal-embed-code")) hint = label;
        }
        check(hint && hint->openExternalLinks() && hint->isHidden(), "Disqus documentation hint missing or visible for Twikoo");
        select->setCurrentIndex(select->findData("disqus"));
        check(!hint->isHidden() && !input->parentWidget()->isHidden(), "Disqus settings or hint hidden when selected");
        check(input->placeholderText() == "my-learning-notes", "Disqus shortname example is missing");
        input->setText("test-comments");
        emit input->textEdited(input->text());
        select->setCurrentIndex(select->findData("none"));
        check(hint->isHidden(), "Disqus hint visible when comments disabled");
        select->setCurrentIndex(select->findData("disqus"));
        dialog.accept();
    }
    book.config = QJsonDocument::fromJson(QJsonDocument(book.config).toJson()).object();
    {
        SiteConfigDialog dialog(book, &parent);
        check(dialog.findChild<QComboBox *>("config_commentPlatform")->currentData() == "disqus", "saved Disqus platform was lost");
        check(dialog.findChild<QLineEdit *>("config_disqus")->text() == "test-comments", "saved Disqus shortname was lost");
        dialog.reject();
    }

    SitePage post;
    post.post = true;
    post.route = "test-post/";
    SiteIndex index;
    index.posts = {&post};
    QString error;
    for (const auto &theme: {"pure", "next", "yilia", "3-jekyll", "custom"}) {
        book.config["theme"] = theme;
        book.config["themeDirectory"] = SITE_THEME_EXAMPLE;
        book.config["commentPlatform"] = "giscus";
        SiteTemplateRenderer templates(book);
        check(templates.load(&error), qPrintable(error));
        const SiteThemeRenderer renderer(book, templates, index);
        const auto html = renderer.renderPage(post);
        check(html.count("https://giscus.app/client.js") == 1, "post must load Giscus once");
        check(html.contains("data-repo=\"owner/comments\"") && html.contains("data-repo-id=\"R_test\"") &&
                  html.contains("data-category=\"Announcements\"") && html.contains("data-category-id=\"DIC_test\""), "Giscus repository or category missing");
        check(html.contains("data-mapping=\"pathname\""), "Giscus must map discussions by pathname");
        post.comments = false;
        check(!renderer.renderPage(post).contains("giscus.app/client.js"), "per-post comment opt-out ignored");
        post.comments = true;
        post.post = false;
        check(!renderer.renderPage(post).contains("giscus.app/client.js"), "standalone page loaded comments");
        post.post = true;
        for (auto it = settings.begin(); it != settings.end(); ++it) {
            book.config[it.key()] = " ";
            check(!renderer.renderPage(post).contains("giscus.app/client.js"), "incomplete settings loaded Giscus");
            book.config[it.key()] = it.value();
        }
        book.config["giscusCategory"] = " General \" & <script> ";
        check(renderer.renderPage(post).contains("data-category=\"General &quot; &amp; &lt;script&gt;\""), "Giscus attributes not trimmed and HTML escaped");
        book.config["giscusCategory"] = settings["giscusCategory"];
        book.config["commentPlatform"] = "twikoo";
        for (const auto &envId: {"https://comments.example.com", "test-env-id", "quote\"\n</script>&"}) {
            book.config["twikooEnvId"] = " " + QString(envId) + " ";
            book.config["twikooRegion"] = " ap-guangzhou ";
            const auto twikoo = renderer.renderPage(post);
            check(twikoo.count("https://cdn.jsdelivr.net/npm/twikoo@2.0.17/dist/twikoo.all.min.js") == 1, "post must load Twikoo once");
            check(twikoo.contains("id=\"tcomment\""), "Twikoo container is missing");
            const auto init = twikoo.section("twikoo.init(", 1, 1).section(");</script>", 0, 0);
            check(!init.contains('<') && !init.contains('&'), "Twikoo settings can break out of script");
            const auto options = QJsonDocument::fromJson(init.toUtf8()).object();
            check(options["envId"] == envId && options["region"] == "ap-guangzhou" && options["el"] == "#tcomment" && options["lang"] == "zh-CN", "Twikoo initialization lost settings");
        }
        book.config["twikooEnvId"] = twikooSettings["twikooEnvId"];
        book.config["twikooRegion"] = " ";
        check(!renderer.renderPage(post).contains("\"region\":"), "blank region should use Twikoo default");
        post.comments = false;
        check(!renderer.renderPage(post).contains("twikoo.init("), "Twikoo ignored per-post opt-out");
        post.comments = true;
        post.post = false;
        check(!renderer.renderPage(post).contains("twikoo.init("), "standalone page loaded Twikoo");
        post.post = true;
        book.config["twikooEnvId"] = " ";
        check(!renderer.renderPage(post).contains("twikoo@"), "empty environment loaded Twikoo");
        book.config["twikooEnvId"] = twikooSettings["twikooEnvId"];
        for (const auto &platform: {"none", "valine", "gitalk"}) {
            book.config["commentPlatform"] = platform;
            const auto disabled = renderer.renderPage(post);
            check(!disabled.contains("giscus.app/client.js") && !disabled.contains("twikoo@") && !disabled.contains("Valine.min.js") && !disabled.contains("gitalk.min.js"), "disabled or retired platform loaded comments");
        }
        book.config["commentPlatform"] = "disqus";
        // Recreate the renderer after domain changes, as site generation does.
        const auto renderDisqus = [&] { return SiteThemeRenderer(book, templates, index).renderPage(post); };
        book.config["disqus"] = " test-comments \n";
        book.config["domain"] = "https://example.com/blog/";
        post.title = "Title \" & </script>";
        const auto disqus = renderDisqus();
        check(disqus.count("https://test-comments.disqus.com/embed.js") == 1, "Disqus shortname was not trimmed or script loaded more than once");
        const auto options = disqusOptions(disqus);
        check(options["identifier"] == post.route, "existing Disqus thread identifier changed");
        check(options["title"] == post.title, "Disqus title was lost");
        check(options["url"] == "https://example.com/blog/test-post/", "Disqus canonical URL lost site subdirectory");
        book.config["domain"] = "http://example.com";
        check(disqusOptions(renderDisqus())["url"] == "http://example.com/test-post/", "Disqus URL requires a trailing slash or HTTPS");
        for (const auto &domain: {"", "/blog/", "ftp://example.com"}) {
            book.config["domain"] = domain;
            check(!disqusOptions(renderDisqus()).contains("url"), "Disqus URL must be absolute HTTP(S)");
        }
        book.config.remove("domain");
        post.comments = false;
        check(!renderDisqus().contains("disqus.com/embed.js"), "Disqus ignored per-post opt-out");
        post.comments = true;
        post.post = false;
        check(!renderDisqus().contains("disqus.com/embed.js"), "standalone page loaded Disqus");
        post.post = true;
        for (const auto &shortname: {"", " ", "https://example.disqus.com", "example.disqus.com", "bad name", "bad\nname", "\"><script>"}) {
            book.config["disqus"] = shortname;
            check(!renderDisqus().contains("disqus.com/embed.js"), "invalid Disqus shortname loaded comments");
        }
    }
    qInfo() << "Site comments checks passed";
}
