#include "SiteTemplateRenderer.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

SiteTemplateRenderer::SiteTemplateRenderer(const Notebook &book) : book_(book) {
    const auto name = book.config["theme"].toString();
    theme_ = name == "next" ? Theme::Next : name == "yilia" ? Theme::Yilia
                                    : name == "3-jekyll"    ? Theme::Three
                                    : name == "custom"      ? Theme::Custom
                                                            : Theme::Pure;
}

QString SiteTemplateRenderer::themeName() const {
    return theme_ == Theme::Next ? "next" : theme_ == Theme::Yilia ? "yilia"
                                    : theme_ == Theme::Three       ? "3-jekyll"
                                                                   : "pure";
}

QString SiteTemplateRenderer::assetsPath() const {
    return theme_ == Theme::Custom ? customThemePath_ + "/source" : ":/site/" + themeName() + "/assets";
}

bool SiteTemplateRenderer::load(QString *error) {
    if (theme_ != Theme::Custom) return true;
    const auto fail = [error](const QString &message) { *error = message; return false; };
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

QString SiteTemplateRenderer::render(const QString &name, const QMap<QString, QString> &values) const {
    QString source;
    if (theme_ == Theme::Custom) source = customTemplates_.value(name);
    else {
        QFile file(":/site/" + themeName() + "/templates/" + name + ".html");
        if (!file.open(QIODevice::ReadOnly)) return {};
        source = QString::fromUtf8(file.readAll());
    }
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
