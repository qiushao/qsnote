#include "SiteNavigationIcons.h"
#include <QDomDocument>
#include <QFile>
#include <QMap>
#include <QRegularExpression>
#include <QSet>
#include <cmath>

namespace {
QList<QDomElement> elements(const QDomDocument &document) {
    QList<QDomElement> result{document.documentElement()};
    for (qsizetype i = 0; i < result.size(); ++i)
        for (auto child = result[i].firstChildElement(); !child.isNull(); child = child.nextSiblingElement()) result.append(child);
    return result;
}
}

bool SiteNavigationIcons::isIconifyName(const QString &name) {
    static const QRegularExpression pattern(QStringLiteral("\\A[a-z0-9]+(?:-[a-z0-9]+)*:[a-z0-9]+(?:-[a-z0-9]+)*\\z"));
    return pattern.match(name).hasMatch();
}

QString SiteNavigationIcons::normalizeSvg(const QByteArray &data, QString *error) {
    const auto fail = [error] {
        if (error) *error = QCoreApplication::translate("SiteNavigationIcons", "图标 SVG 无效或包含不支持的内容");
        return QString();
    };
    QDomDocument document;
    if (data.isEmpty() || data.size() > maximumSvgBytes || data.toLower().contains("<!doctype") || !document.setContent(data, true)) return fail();
    auto root = document.documentElement();
    if (root.tagName() != "svg" || root.namespaceURI() != "http://www.w3.org/2000/svg") return fail();
    const auto box = root.attribute("viewBox").split(QRegularExpression("[ ,\\s]+"), Qt::SkipEmptyParts);
    if (box.size() != 4) return fail();
    for (int i = 0; i < box.size(); ++i) {
        bool ok = false;
        const auto value = box[i].toDouble(&ok);
        if (!ok || !std::isfinite(value) || (i >= 2 && value <= 0)) return fail();
    }
    static const QSet<QString> tags{"svg", "g", "defs", "symbol", "use", "path", "rect", "circle", "ellipse", "line", "polyline", "polygon",
        "linearGradient", "radialGradient", "stop", "clipPath", "mask", "pattern", "filter", "feGaussianBlur", "feOffset", "feBlend", "feColorMatrix",
        "feComposite", "feFlood", "feMerge", "feMergeNode", "feDropShadow", "animate", "animateTransform", "animateMotion", "mpath", "set", "title", "desc"};
    static const QSet<QString> attributes{"id", "viewBox", "width", "height", "preserveAspectRatio", "fill", "fill-rule", "fill-opacity", "stroke", "stroke-width",
        "stroke-linecap", "stroke-linejoin", "stroke-miterlimit", "stroke-dasharray", "stroke-dashoffset", "stroke-opacity", "opacity", "color", "transform",
        "d", "x", "y", "x1", "x2", "y1", "y2", "cx", "cy", "r", "rx", "ry", "fx", "fy", "fr", "points", "pathLength", "vector-effect",
        "clip-path", "clip-rule", "mask", "mask-type", "maskUnits", "maskContentUnits", "gradientUnits", "gradientTransform", "spreadMethod", "offset", "stop-color", "stop-opacity",
        "patternUnits", "patternContentUnits", "patternTransform", "filter", "filterUnits", "primitiveUnits", "in", "in2", "result", "stdDeviation", "dx", "dy", "mode", "operator",
        "k1", "k2", "k3", "k4", "flood-color", "flood-opacity", "color-interpolation-filters", "href", "attributeName", "attributeType", "type", "begin", "end", "dur",
        "min", "max", "repeatCount", "repeatDur", "restart", "from", "to", "by", "values", "keyTimes", "keySplines", "keyPoints", "calcMode", "additive", "accumulate", "rotate", "origin"};
    // Only geometry and paint may be animated; links and arbitrary attributes cannot be changed by SMIL.
    static const QSet<QString> animated{"d", "x", "y", "x1", "x2", "y1", "y2", "cx", "cy", "r", "rx", "ry", "width", "height", "points", "transform",
        "fill", "fill-opacity", "stroke", "stroke-width", "stroke-dasharray", "stroke-dashoffset", "stroke-opacity", "opacity", "color", "offset", "stop-color", "stop-opacity"};
    static const QRegularExpression localUrl(QStringLiteral("url\\(\\s*['\"]?#[A-Za-z0-9_.:-]+['\"]?\\s*\\)"), QRegularExpression::CaseInsensitiveOption);
    QSet<QString> ids;
    for (const auto &element: elements(document)) {
        if (element.namespaceURI() != root.namespaceURI() || !tags.contains(element.tagName())) return fail();
        const auto id = element.attribute("id");
        if (!id.isEmpty()) {
            if (!QRegularExpression("\\A[A-Za-z0-9_.:-]+\\z").match(id).hasMatch() || ids.contains(id)) return fail();
            ids.insert(id);
        }
        const auto attributeMap = element.attributes();
        for (int i = 0; i < attributeMap.size(); ++i) {
            const auto attribute = attributeMap.item(i).toAttr();
            const auto name = attribute.localName().isEmpty() ? attribute.name() : attribute.localName();
            const auto value = attribute.value();
            if (attribute.name() == "xmlns" && value == "http://www.w3.org/2000/svg") continue;
            if (attribute.name() == "xmlns:xlink" && value == "http://www.w3.org/1999/xlink") continue;
            if ((!attribute.namespaceURI().isEmpty() && !(attribute.namespaceURI() == "http://www.w3.org/1999/xlink" && name == "href")) || !attributes.contains(name)) return fail();
            if (name == "href" && !QRegularExpression("\\A#[A-Za-z0-9_.:-]+\\z").match(value).hasMatch()) return fail();
            auto remaining = value;
            remaining.remove(localUrl);
            if (remaining.contains("url", Qt::CaseInsensitive) || value.contains('\\') || value.contains('<')) return fail();
            if (name == "attributeName" && !animated.contains(value)) return fail();
        }
    }
    root.setAttribute("width", "24");
    root.setAttribute("height", "24");
    QDomDocument clean;
    clean.appendChild(clean.importNode(root, true));
    return clean.toString(-1);
}

bool SiteNavigationIcons::validate(const QJsonObject &item, QString *error) {
    const auto id = item["icon"].toString();
    if ((!item.contains("icon") || item["icon"].isString()) && (id.isEmpty() || !resource(id).isEmpty())) return true;
    if (isIconifyName(id)) {
        if (!normalizeSvg(item["iconSvg"].toString().toUtf8()).isEmpty()) return true;
        *error = QCoreApplication::translate("SiteNavigationIcons", "请下载并预览此 Iconify 图标：%1").arg(id);
    } else *error = QCoreApplication::translate("SiteDocuments", "导航图标无效，请重新选择：%1").arg(item["title"].toString());
    return false;
}

QString SiteNavigationIcons::markup(const QJsonObject &item, const QString &prefix) {
    const auto id = item["icon"].toString();
    QString svg;
    if (isIconifyName(id)) svg = normalizeSvg(item["iconSvg"].toString().toUtf8());
    else if (const auto path = resource(id); !path.isEmpty()) {
        QFile file(path);
        if (file.open(QIODevice::ReadOnly)) svg = QString::fromUtf8(file.readAll()).replace("#6e6e7b", "currentColor");
    }
    if (svg.isEmpty()) return {};
    QDomDocument document;
    if (!document.setContent(svg, false)) return {};
    const auto nodes = elements(document);
    QMap<QString, QString> ids;
    for (const auto &node: nodes) if (node.hasAttribute("id")) ids.insert(node.attribute("id"), prefix + QString::number(ids.size()));
    for (const auto &node: nodes) {
        const auto attributes = node.attributes();
        for (int i = 0; i < attributes.size(); ++i) {
            auto attribute = attributes.item(i).toAttr();
            auto value = attribute.value();
            for (auto it = ids.cbegin(); it != ids.cend(); ++it) {
                if (attribute.name() == "id" && value == it.key()) value = it.value();
                else if ((attribute.name() == "href" || attribute.name() == "xlink:href") && value == '#' + it.key()) value = '#' + it.value();
                else {
                    value.replace(QRegularExpression(R"(url\(\s*['"]?#)" + QRegularExpression::escape(it.key()) + R"(['"]?\s*\))"), "url(#" + it.value() + ')');
                    if (attribute.name() == "begin" || attribute.name() == "end")
                        value.replace(QRegularExpression("(^|;\\s*)" + QRegularExpression::escape(it.key()) + "\\."), "\\1" + it.value() + '.');
                }
            }
            attribute.setValue(value);
        }
    }
    auto root = document.documentElement();
    root.setAttribute("class", "vp-nav-icon");
    root.setAttribute("aria-hidden", "true");
    root.setAttribute("focusable", "false");
    return document.toString(-1);
}
