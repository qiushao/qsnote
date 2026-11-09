#include "SiteTemplateRenderer.h"
#include <QCoreApplication>
#include <QDate>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocale>
#include <QRegularExpression>
#include <QSet>
#include <functional>

namespace {
bool relativePath(const QString &path) {
    if (path.isEmpty() || path.startsWith('/') || path.contains('\\') || path.contains(':')) return false;
    for (const auto &part: path.split('/')) if (part.isEmpty() || part == "." || part == "..") return false;
    return true;
}
bool truthy(const QJsonValue &value) {
    if (value.isBool()) return value.toBool();
    if (value.isDouble()) return value.toDouble() != 0;
    if (value.isString()) return !value.toString().isEmpty();
    if (value.isArray()) return !value.toArray().isEmpty();
    if (value.isObject()) return !value.toObject().isEmpty();
    return false;
}
QJsonValue lookup(const QList<QJsonValue> &scopes, const QString &name) {
    if (name == ".") return scopes.last();
    const auto parts = name.split('.');
    for (auto it = scopes.crbegin(); it != scopes.crend(); ++it) {
        if (!it->isObject() || !it->toObject().contains(parts.first())) continue;
        auto value = it->toObject().value(parts.first());
        for (qsizetype i = 1; i < parts.size(); ++i) value = value.toObject().value(parts[i]);
        return value;
    }
    return {};
}
QString scalar(const QJsonValue &value) {
    if (value.isString()) return value.toString();
    if (value.isBool()) return value.toBool() ? "true" : "false";
    if (value.isDouble()) return QString::number(value.toDouble(), 'g', 16);
    return {};
}
}

SiteTemplateRenderer::SiteTemplateRenderer(const Notebook &book) : book_(book) {}

QString SiteTemplateRenderer::assetsPath() const { return themePath_ + '/' + assetsDirectory_; }

bool SiteTemplateRenderer::load(QString *error) {
    templates_.clear();
    entries_ = {};
    options_ = {};
    const auto fail = [error](const QString &path, const QString &detail) {
        if (error) *error = QCoreApplication::translate("SiteGenerator", "主题加载失败：%1：%2").arg(path, detail);
        return false;
    };
    const auto name = book_.config["theme"].toString("pure");
    if (name == "custom") {
        const auto configured = book_.config["themeDirectory"].toString().trimmed();
        if (configured.isEmpty()) return fail("themeDirectory", QCoreApplication::translate("SiteGenerator", "请选择自定义主题目录"));
        themePath_ = QDir::cleanPath(QDir(book_.path).absoluteFilePath(configured));
    } else {
        if (!QRegularExpression("\\A[a-zA-Z0-9_-]+\\z").match(name).hasMatch()) return fail(name, QCoreApplication::translate("SiteGenerator", "主题资源名称无效"));
        themePath_ = ":/site/" + name;
    }
    const auto path = themePath_ + "/theme.json";
    QFile file(path);
    if (!file.exists()) return fail(path, QCoreApplication::translate("SiteGenerator", "缺少 theme.json，请按主题规范迁移旧主题"));
    if (!file.open(QIODevice::ReadOnly)) return fail(path, file.errorString());
    QJsonParseError jsonError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &jsonError);
    if (jsonError.error != QJsonParseError::NoError || !document.isObject()) return fail(path, QCoreApplication::translate("SiteGenerator", "JSON 对象无效：%1").arg(jsonError.errorString()));
    const auto config = document.object();
    const QSet<QString> keys{"version", "templates", "assets", "entries", "options"};
    for (auto it = config.begin(); it != config.end(); ++it) if (!keys.contains(it.key())) return fail(path, QCoreApplication::translate("SiteGenerator", "未知配置项：%1").arg(it.key()));
    if (config["version"] != QJsonValue(1)) return fail(path, QCoreApplication::translate("SiteGenerator", "不支持的主题规范版本，应为 1"));
    for (const auto &key: {"templates", "assets"})
        if (!config[key].isString() || !relativePath(config[key].toString())) return fail(path, QCoreApplication::translate("SiteGenerator", "%1 必须是相对目录").arg(key));
    assetsDirectory_ = config["assets"].toString();
    if (!QFileInfo(assetsPath()).isDir()) return fail(assetsPath(), QCoreApplication::translate("SiteGenerator", "找不到主题资源目录"));
    if (!config["entries"].isObject()) return fail(path, QCoreApplication::translate("SiteGenerator", "entries 必须是对象"));
    entries_ = config["entries"].toObject();
    const QStringList entryTypes{"layout", "index", "post", "page", "archive", "category", "tag", "categories", "tags", "links", "document", "documentNode"};
    for (const auto &key: entryTypes) if (!entries_[key].isString() || !relativePath(entries_[key].toString())) return fail(path, QCoreApplication::translate("SiteGenerator", "entries.%1 必须指定模板名称").arg(key));
    for (auto it = entries_.begin(); it != entries_.end(); ++it) if (!entryTypes.contains(it.key())) return fail(path, QCoreApplication::translate("SiteGenerator", "未知页面入口：%1").arg(it.key()));
    if (config.contains("options") && !config["options"].isObject()) return fail(path, QCoreApplication::translate("SiteGenerator", "options 必须是对象"));
    options_ = config["options"].toObject();
    for (auto it = options_.begin(); it != options_.end(); ++it) {
        const auto key = it.key(); const auto value = it.value();
        bool valid = false;
        if (key == "excerptLength") valid = value.isDouble() && value.toDouble() == value.toInt() && value.toInt() >= 3;
        else if (key == "excerptFormat") valid = value == QJsonValue("text") || value == QJsonValue("html");
        else if (key == "singlePostIndex" || key == "rootRelativeAvatar") valid = value.isBool();
        else if (key == "defaultAvatar") valid = value.isString();
        else if (key == "groupByYear" || key == "searchEscapeFields") {
            valid = value.isArray();
            const QStringList allowed = key == "groupByYear" ? QStringList{"index", "archive", "category", "tag"} : QStringList{"title", "content"};
            for (const auto &item: value.toArray()) if (!item.isString() || !allowed.contains(item.toString())) valid = false;
        }
        if (!valid) return fail(path, QCoreApplication::translate("SiteGenerator", "主题行为配置无效：%1").arg(key));
    }
    const auto directory = themePath_ + '/' + config["templates"].toString();
    QDirIterator files(directory, {"*.html"}, QDir::Files, QDirIterator::Subdirectories);
    QMap<QString, QString> paths;
    while (files.hasNext()) {
        const auto templatePath = files.next();
        QFile input(templatePath);
        if (!input.open(QIODevice::ReadOnly)) return fail(templatePath, input.errorString());
        const auto source = QString::fromUtf8(input.readAll());
        if (source.trimmed().isEmpty() || source.contains("<%") || source.contains("{%")) return fail(templatePath, QCoreApplication::translate("SiteGenerator", "模板为空或包含不支持的模板语法"));
        const auto key = QDir(directory).relativeFilePath(templatePath).chopped(5);
        QString detail; qsizetype offset = 0; QList<Node> nodes;
        if (!parse(source, offset, nodes, {}, &detail)) return fail(templatePath, detail);
        templates_.insert(key, nodes); paths.insert(key, templatePath);
    }
    for (auto it = entries_.begin(); it != entries_.end(); ++it)
        if (!templates_.contains(it.value().toString())) return fail(directory + '/' + it.value().toString() + ".html", QCoreApplication::translate("SiteGenerator", "找不到页面入口模板：%1").arg(it.key()));
    QSet<QString> active, complete;
    std::function<bool(const QString &)> visit;
    std::function<bool(const QList<Node> &, const QString &)> inspect = [&](const QList<Node> &nodes, const QString &owner) {
        for (const auto &node: nodes) {
            if (node.kind == Node::Kind::Partial) {
                if (!templates_.contains(node.value)) return fail(paths[owner], QCoreApplication::translate("SiteGenerator", "找不到引用的模板片段：%1").arg(node.value));
                if (active.contains(node.value)) return fail(paths[owner], QCoreApplication::translate("SiteGenerator", "模板片段存在引用环：%1").arg(node.value));
                if (!visit(node.value)) return false;
            }
            if (!inspect(node.children, owner)) return false;
        }
        return true;
    };
    visit = [&](const QString &key) {
        if (complete.contains(key)) return true;
        active.insert(key);
        if (!inspect(templates_[key], key)) return false;
        active.remove(key); complete.insert(key); return true;
    };
    for (auto it = templates_.cbegin(); it != templates_.cend(); ++it) if (!visit(it.key())) return false;
    return true;
}

// Template blocks form a finite syntax tree.
// NOLINTNEXTLINE(misc-no-recursion)
bool SiteTemplateRenderer::parse(const QString &source, qsizetype &offset, QList<Node> &nodes, const QString &block, QString *error) const {
    const auto fail = [&](const QString &detail) { *error = QCoreApplication::translate("SiteGenerator", "%1（位置 %2）").arg(detail).arg(offset); return false; };
    static const QRegularExpression field(R"(\A(?:[A-Za-z_][A-Za-z0-9_]*(?:\.[A-Za-z_][A-Za-z0-9_]*)*|\.)\z)");
    while (offset < source.size()) {
        const auto start = source.indexOf("{{", offset);
        if (start < 0) { nodes.append({Node::Kind::Text, source.mid(offset), {}, {}, {}}); offset = source.size(); break; }
        if (start > offset) nodes.append({Node::Kind::Text, source.mid(offset, start - offset), {}, {}, {}});
        const bool raw = source.mid(start, 3) == "{{{";
        const auto end = source.indexOf(raw ? "}}}" : "}}", start + (raw ? 3 : 2));
        if (end < 0) return fail(QCoreApplication::translate("SiteGenerator", "占位符未闭合"));
        auto token = source.mid(start + (raw ? 3 : 2), end - start - (raw ? 3 : 2)).trimmed();
        offset = end + (raw ? 3 : 2);
        Node node; node.kind = raw ? Node::Kind::Raw : Node::Kind::Value;
        if (!raw && token.startsWith('/')) {
            if (block.isEmpty() || token.mid(1).trimmed() != block) return fail(QCoreApplication::translate("SiteGenerator", "条件块结束标记不匹配：%1").arg(token));
            return true;
        }
        if (!raw && token.startsWith('>')) {
            node.kind = Node::Kind::Partial; node.value = token.mid(1).trimmed();
            if (!relativePath(node.value)) return fail(QCoreApplication::translate("SiteGenerator", "模板片段引用无效：%1").arg(token));
        } else {
            if (!raw && (token.startsWith('#') || token.startsWith('^'))) {
                node.kind = token.startsWith('#') ? Node::Kind::Section : Node::Kind::Inverted; token = token.mid(1).trimmed();
            }
            static const QRegularExpression dateFilter(R"(\|date(?:\(([A-Za-z0-9_-]+)\))?:)");
            const auto filter = dateFilter.match(token);
            if (filter.hasMatch() && (node.kind == Node::Kind::Value || node.kind == Node::Kind::Raw)) {
                node.dateFormat = token.mid(filter.capturedEnd());
                node.dateLocale = filter.captured(1);
                token.truncate(filter.capturedStart());
                if (node.dateFormat.isEmpty()) return fail(QCoreApplication::translate("SiteGenerator", "日期格式不能为空"));
                if (!node.dateLocale.isEmpty() && node.dateLocale != "C" && QLocale(node.dateLocale).language() == QLocale::C)
                    return fail(QCoreApplication::translate("SiteGenerator", "日期区域设置无效：%1").arg(node.dateLocale));
            }
            node.value = token.trimmed();
            if (!field.match(node.value).hasMatch()) return fail(QCoreApplication::translate("SiteGenerator", "模板字段无效：%1").arg(token));
            if ((node.kind == Node::Kind::Section || node.kind == Node::Kind::Inverted) && !parse(source, offset, node.children, node.value, error)) return false;
        }
        nodes.append(node);
    }
    return block.isEmpty() || fail(QCoreApplication::translate("SiteGenerator", "条件块未闭合：%1").arg(block));
}

// Partial cycles are rejected during load; evaluation only walks the parsed tree.
// NOLINTNEXTLINE(misc-no-recursion)
QString SiteTemplateRenderer::evaluate(const QList<Node> &nodes, const QList<QJsonValue> &scopes) const {
    QString result;
    for (const auto &node: nodes) {
        if (node.kind == Node::Kind::Text) { result += node.value; continue; }
        if (node.kind == Node::Kind::Partial) { result += evaluate(templates_.value(node.value), scopes); continue; }
        const auto value = lookup(scopes, node.value);
        if (node.kind == Node::Kind::Section) {
            if (value.isArray()) {
                for (const auto &item: value.toArray()) { auto nested = scopes; nested.append(item); result += evaluate(node.children, nested); }
            } else if (truthy(value)) {
                auto nested = scopes;
                if (value.isObject()) nested.append(value);
                result += evaluate(node.children, nested);
            }
        } else if (node.kind == Node::Kind::Inverted) {
            if (!truthy(value)) result += evaluate(node.children, scopes);
        } else {
            auto text = scalar(value);
            if (!node.dateFormat.isEmpty()) {
                const auto date = QDate::fromString(text.left(10), Qt::ISODate);
                text = node.dateLocale.isEmpty() ? date.toString(node.dateFormat) : QLocale(node.dateLocale).toString(date, node.dateFormat);
            }
            result += node.kind == Node::Kind::Raw ? text : text.toHtmlEscaped().replace('\'', "&#39;");
        }
    }
    return result;
}

QString SiteTemplateRenderer::render(const QString &name, const QJsonObject &context) const { return evaluate(templates_.value(name), {context}); }
QString SiteTemplateRenderer::renderEntry(const QString &type, const QJsonObject &context) const { return render(entries_[type].toString(), context); }
