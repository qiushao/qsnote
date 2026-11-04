#pragma once

#include <QColor>
#include <QString>
#include <QStringList>

// 应用外观主题：主题模式（浅色/深色/跟随系统）与主题色。
// 配色参考 Gridea Pro 的 tokens.css（OKLCH 已换算为 sRGB）。
namespace Theme {

// Appearance/mode 的原始值：light / dark / system
QString mode();
// 解析后的实际模式（system 时读取系统外观）
bool isDark();

QStringList accentIds();
QString accentId();
QColor accentColor(const QString &id, bool dark);
QColor accentColor();
// 当前主题的令牌色值，token 为不含 @ 的令牌名，如 "window"、"accentSoft"
QColor color(const QString &token);
QColor color(const QString &token, const QString &id, bool dark);

// 将样式表中的 @token 占位符替换为当前主题的色值
QString process(const QString &stylesheet);

}// namespace Theme
