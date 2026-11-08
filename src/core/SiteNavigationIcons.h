#pragma once

#include <QCoreApplication>
#include <QJsonObject>
#include <QList>
#include <QString>

namespace SiteNavigationIcons {
struct Icon { const char *id; const char *label; };
inline const QList<Icon> &items() {
    static const QList<Icon> icons{
        {"home", QT_TRANSLATE_NOOP("SiteNavigationIcons", "首页")},
        {"book", QT_TRANSLATE_NOOP("SiteNavigationIcons", "文档")},
        {"folder", QT_TRANSLATE_NOOP("SiteNavigationIcons", "文件夹")},
        {"tag", QT_TRANSLATE_NOOP("SiteNavigationIcons", "标签")},
        {"archive", QT_TRANSLATE_NOOP("SiteNavigationIcons", "归档")},
        {"link", QT_TRANSLATE_NOOP("SiteNavigationIcons", "链接")},
        {"user", QT_TRANSLATE_NOOP("SiteNavigationIcons", "用户")},
        {"code", QT_TRANSLATE_NOOP("SiteNavigationIcons", "代码")},
        {"settings", QT_TRANSLATE_NOOP("SiteNavigationIcons", "设置")},
        {"search", QT_TRANSLATE_NOOP("SiteNavigationIcons", "搜索")},
        {"globe", QT_TRANSLATE_NOOP("SiteNavigationIcons", "网站")},
        {"star", QT_TRANSLATE_NOOP("SiteNavigationIcons", "星标")},
        {"github", QT_TRANSLATE_NOOP("SiteNavigationIcons", "GitHub")}
    };
    return icons;
}
inline QString resource(const QString &id) {
    for (const auto &icon: items()) if (id == QLatin1String(icon.id)) return ":/site/plume/assets/nav-icons/" + id + ".svg";
    return {};
}

// Iconify SVGs are cached on the navigation item, so generation stays offline.
constexpr int maximumSvgBytes = 256 * 1024;
bool isIconifyName(const QString &name);
QString normalizeSvg(const QByteArray &data, QString *error = nullptr);
bool validate(const QJsonObject &item, QString *error);
QString markup(const QJsonObject &item, const QString &prefix);
}
