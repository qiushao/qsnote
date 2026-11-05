#include "core/NotebookCategories.h"
#include "core/SiteThemeRenderer.h"
#include "ui/SiteConfigDialog.h"
#include <QApplication>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QListWidget>
#include <QPushButton>
#include <QRegularExpression>
#include <cstdlib>

namespace {
void check(bool condition, const char *message) {
    if (condition) return;
    qCritical() << message;
    std::exit(1);
}

void checkLinks(const QString &html, const QString &kind, const QStringList &names) {
    qsizetype previous = -1;
    for (const auto &name: names) {
        const auto position = html.indexOf(QRegularExpression(R"(<a\b[^>]*\bhref=")" + QRegularExpression::escape("/" + kind + "/" + name + "/") + '"'));
        if (position <= previous) qCritical() << kind << name << position << previous;
        check(position > previous, "taxonomy links are missing or out of order");
        previous = position;
    }
}
}// namespace

int main(int argc, char **argv) {
    const QApplication app(argc, argv);
    Notebook book;
    book.type = "site";
    book.config["categories"] = QJsonArray{"Alpha", "Zulu", "Empty"};
    check(NotebookCategories::site(book) == QStringList{"default", "Alpha", "Zulu", "Empty"}, "legacy config lost default category");
    QWidget parent;
    parent.resize(1200, 900);
    {
        SiteConfigDialog dialog(book, &parent);
        auto *list = dialog.findChild<QListWidget *>("siteCategories");
        auto *up = dialog.findChild<QPushButton *>("moveSiteCategoryUp");
        auto *down = dialog.findChild<QPushButton *>("moveSiteCategoryDown");
        check(list && up && down, "category order controls are missing");
        check(!up->isEnabled() && down->isEnabled(), "first category move boundaries are incorrect");
        down->click();
        check(list->currentRow() == 1 && list->currentItem()->text() == "default", "moving default lost selection");
        list->setCurrentRow(3);
        check(up->isEnabled() && !down->isEnabled(), "last category move boundaries are incorrect");
        dialog.reject();
        check(NotebookCategories::site(book).first() == "default", "cancel applied category order");
    }
    {
        SiteConfigDialog dialog(book, &parent);
        auto *list = dialog.findChild<QListWidget *>("siteCategories");
        auto *up = dialog.findChild<QPushButton *>("moveSiteCategoryUp");
        list->setCurrentRow(2);
        up->click();
        up->click();
        bool changed = false;
        QObject::connect(&dialog, &SiteConfigDialog::categoriesChanged, [&changed] { changed = true; });
        dialog.accept();
        check(changed, "saving category order did not request tree refresh");
    }
    const QStringList expected{"Zulu", "default", "Alpha", "Empty"};
    check(NotebookCategories::site(book) == expected, "save lost manual category order");
    book.config = QJsonDocument::fromJson(QJsonDocument(book.config).toJson()).object();
    {
        SiteConfigDialog dialog(book, &parent);
        auto *list = dialog.findChild<QListWidget *>("siteCategories");
        for (int i = 0; i < expected.size(); ++i)
            check(list->item(i)->text() == expected.at(i), "reopened config lost manual category order");
    }

    SitePage post;
    post.post = true;
    post.route = "test-post/";
    SiteIndex index;
    for (const auto &name: {"Alpha", "Zulu", "default", "External"}) index.categories[name].append(&post);
    for (const auto &name: {"Zulu", "Alpha"}) index.tags[name].append(&post);
    SitePage categories;
    categories.route = "categories/";
    SitePage tags;
    tags.route = "tags/";
    QString error;
    for (const auto &theme: {"pure", "next", "yilia", "3-jekyll", "custom"}) {
        qInfo() << "Checking theme" << theme;
        book.config["theme"] = theme;
        book.config["themeDirectory"] = SITE_THEME_EXAMPLE;
        SiteTemplateRenderer templates(book);
        check(templates.load(&error), qPrintable(error));
        const SiteThemeRenderer renderer(book, templates, index);
        const auto html = renderer.renderPage(categories);
        checkLinks(html, "categories", {"Zulu", "default", "Alpha", "External"});
        check(!html.contains("/categories/Empty/"), "empty category appeared in generated site");
        checkLinks(renderer.renderPage(tags), "tags", {"Alpha", "Zulu"});
        if (QString(theme) == "pure") {
            const auto sidebar = html.mid(html.indexOf("<ul class=\"category-list\">"));
            checkLinks(sidebar, "categories", {"Zulu", "default", "Alpha", "External"});
            checkLinks(renderer.renderListing("categories/Alpha/", "Alpha", {&post}, {&post}, 1, 1),
                       "categories", {"Zulu", "default", "Alpha", "External"});
        }
    }
    check(NotebookCategories::rename(book, "Zulu", "Renamed", &error), qPrintable(error));
    check(NotebookCategories::site(book).first() == "Renamed", "rename changed category position");
    check(NotebookCategories::remove(book, "Alpha", &error), qPrintable(error));
    check(NotebookCategories::create(book, "New", &error), qPrintable(error));
    check(NotebookCategories::site(book) == QStringList{"Renamed", "default", "Empty", "New"}, "category edits changed manual order");
    book.type = "todo";
    book.config["categories"] = QJsonArray{"Other", QStringLiteral("默认")};
    check(NotebookCategories::todo(book) == QStringList{QStringLiteral("默认"), "Other"}, "todo default category no longer comes first");
    qInfo() << "Site category order checks passed";
}
