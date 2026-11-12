#include "Theme.h"

#include <QGuiApplication>
#include <QHash>
#include <QSettings>
#include <cmath>
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
#include <QStyleHints>
#endif

namespace Theme {
namespace {

struct AccentDef {
    const char *id;
    const char *light;
    const char *dark;
    // 浅色模式下选中项文字的深一档颜色；为空则由种子色加深派生
    const char *textLight;
    // 浅色模式下强调色上的文字颜色；为空则为白色
    const char *onLight;
};

// 黑白配及取自 Gridea Pro tokens.css 的「四时山水」主题色
constexpr AccentDef kAccents[] = {
        {"monochrome", "#0066cc", "#6da6e0", nullptr, nullptr},
        {"green", "#338a66", "#69b793", nullptr, nullptr},
        {"rose", "#b45151", "#e4817e", nullptr, nullptr},
        {"sakura", "#c37292", "#f4a1c1", "#a65778", nullptr},
        {"sunset", "#d47b4a", "#d47b4a", "#b1582b", nullptr},
        {"amber", "#be9d54", "#eecd87", "#9a6418", "#231a06"},
        {"cyan", "#29889c", "#63b6c9", nullptr, nullptr},
        {"blue", "#3d78b2", "#6da6e0", nullptr, nullptr},
        {"purple", "#8b39ff", "#9254de", nullptr, nullptr},
        {"ink", "#000000", "#f5f5f5", nullptr, nullptr},
};

const AccentDef &accentDef(const QString &id) {
    for (const auto &def: kAccents) {
        if (id == QLatin1String(def.id)) return def;
    }
    return kAccents[1];// green
}

QColor mix(const QColor &base, const QColor &tint, double t) {
    const auto f = static_cast<float>(t);
    return QColor::fromRgbF(base.redF() + (tint.redF() - base.redF()) * f,
                            base.greenF() + (tint.greenF() - base.greenF()) * f,
                            base.blueF() + (tint.blueF() - base.blueF()) * f);
}

QColor contrastingText(const QColor &color) {
    const auto linear = [](double value) { return value <= 0.04045 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4); };
    const double luminance = 0.2126 * linear(color.redF()) + 0.7152 * linear(color.greenF()) + 0.0722 * linear(color.blueF());
    return luminance > 0.179 ? QColor(Qt::black) : QColor(Qt::white);
}

}// namespace

QString mode() {
    return QSettings().value("Appearance/mode", "system").toString();
}

bool isDark() {
    const QString value = mode();
    if (value == "dark") return true;
    if (value == "light") return false;
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
#else
    return false;
#endif
}

QStringList accentIds() {
    QStringList ids;
    for (const auto &def: kAccents) ids << QLatin1String(def.id);
    ids << "custom";
    return ids;
}

QString accentId() {
    const QString id = QSettings().value("Appearance/accent", "sunset").toString();
    return id == "custom" ? id : QString::fromLatin1(accentDef(id).id);
}

QColor accentColor(const QString &id, bool dark) {
    if (id == "custom") return customOverrides(dark).value("accent", customSeed());
    const auto &def = accentDef(id);
    return {QLatin1String(dark ? def.dark : def.light)};
}

QColor accentColor() {
    return accentColor(accentId(), isDark());
}

QColor customSeed() {
    const QColor seed(QSettings().value("Appearance/customAccent", "#d47b4a").toString());
    return seed.isValid() ? seed : QColor("#d47b4a");
}

QHash<QString, QColor> customOverrides(bool dark) {
    QSettings settings;
    settings.beginGroup(dark ? "Appearance/customDark" : "Appearance/customLight");
    QHash<QString, QColor> overrides;
    for (const auto *key: {"window", "text", "focus", "accent"}) {
        const QColor value(settings.value(key).toString());
        if (value.isValid()) overrides.insert(key, value);
    }
    return overrides;
}

namespace {

// 中性色混入种子色的比例：整体氛围（窗口、面板、边框）随主题色轻微偏移，
// 与 Gridea Pro 的做法一致，避免只改强调色导致界面割裂。
double tintRatio(const QString &token, bool dark) {
    static const QHash<QString, QPair<double, double>> ratios = {
            {"@window", {0.07, 0.05}},     {"@surface", {0.06, 0.035}},  {"@panel", {0.09, 0.06}},
            {"@panelAlt", {0.08, 0.05}},   {"@disabledBg", {0.05, 0.04}},{"@border", {0.14, 0.10}},
            {"@borderLight", {0.12, 0.08}},{"@inputBorder", {0.13, 0.09}},{"@cardBorder", {0.13, 0.09}},
            {"@hover", {0.12, 0.07}},      {"@hoverBorder", {0.22, 0.16}},{"@hoverSoft", {0.10, 0.06}},
            {"@tabHover", {0.10, 0.06}},   {"@scrollbar", {0.18, 0.12}}, {"@scrollbarHover", {0.26, 0.18}},
            {"@inputHoverBorder", {0.28, 0.20}},{"@checkBorder", {0.24, 0.16}},
    };
    const auto ratio = ratios.value(token);
    return dark ? ratio.first : ratio.second;
}

// 当前主题的完整令牌表
QHash<QString, QString> buildTokens(bool dark, const QString &id, const QColor &seed = {}, const QHash<QString, QColor> &overrides = {}) {
    const auto &accent = accentDef(id);
    const auto custom = id == "custom" ? (seed.isValid() ? overrides : customOverrides(dark)) : QHash<QString, QColor>();
    const QColor seedColor = id == "custom" ? (seed.isValid() ? seed : customSeed()) : Theme::accentColor(id, dark);
    const QColor accentColor = custom.value("accent", seedColor);

    QHash<QString, QString> tokens;
    if (dark) {
        // 中性色参考 Gridea Pro dark tokens（背景 L≈18.5%、卡片 22.5%、边框 30%、正文 92%）
        tokens["@window"] = "#1b1b1e";
        tokens["@surface"] = "#242428";
        tokens["@panel"] = "#1f1f23";
        tokens["@panelAlt"] = "#212125";
        tokens["@disabledBg"] = "#202024";
        tokens["@border"] = "#2e2e34";
        tokens["@borderLight"] = "#2a2a30";
        tokens["@inputBorder"] = "#34343b";
        tokens["@cardBorder"] = "#2b2b31";
        tokens["@text"] = "#e8e8ea";
        tokens["@textStrong"] = "#f3f3f5";
        tokens["@textSecondary"] = "#b4b4bc";
        tokens["@textDim"] = "#9a9aa2";
        tokens["@textDisabled"] = "#5a5a62";
        tokens["@textFaint"] = "#4a4a52";
        tokens["@hover"] = "#2c2c33";
        tokens["@hoverBorder"] = "#43434c";
        tokens["@hoverSoft"] = "#292930";
        tokens["@tabHover"] = "#2b2b32";
        tokens["@scrollbar"] = "#3c3c44";
        tokens["@scrollbarHover"] = "#54545e";
        tokens["@inputHoverBorder"] = "#44444d";
        tokens["@checkBorder"] = "#4e4e57";
        tokens["@tooltipBg"] = "#e8e8ea";
        tokens["@tooltipText"] = "#1b1b1e";
    } else {
        tokens["@window"] = "#fdfdfc";
        tokens["@surface"] = "#ffffff";
        tokens["@panel"] = "#f3f3f1";
        tokens["@panelAlt"] = "#f8f8f6";
        tokens["@disabledBg"] = "#f8f8fa";
        tokens["@border"] = "#e9e9ee";
        tokens["@borderLight"] = "#efeff3";
        tokens["@inputBorder"] = "#e2e2e9";
        tokens["@cardBorder"] = "#e5e3eb";
        tokens["@text"] = "#242424";
        tokens["@textStrong"] = "#303030";
        tokens["@textSecondary"] = "#525258";
        tokens["@textDim"] = "#606066";
        tokens["@textDisabled"] = "#a3a1ae";
        tokens["@textFaint"] = "#c3c1cd";
        tokens["@hover"] = "#f1f0f5";
        tokens["@hoverBorder"] = "#d2cfdd";
        tokens["@hoverSoft"] = "#eeedf3";
        tokens["@tabHover"] = "#f0eef5";
        tokens["@scrollbar"] = "#d9d7e1";
        tokens["@scrollbarHover"] = "#b5b1c2";
        tokens["@inputHoverBorder"] = "#cfccd9";
        tokens["@checkBorder"] = "#c0bbcd";
        tokens["@tooltipBg"] = "#303030";
        tokens["@tooltipText"] = "#ffffff";
    }

    if (QLatin1String(accent.id) == "monochrome") {
        // 黑白配使用中性灰阶，蓝色仅用于强调状态。
        for (auto it = tokens.begin(); it != tokens.end(); ++it) {
            const int gray = qGray(QColor(it.value()).rgb());
            it.value() = QColor(gray, gray, gray).name();
        }
        if (!dark) {
            tokens["@window"] = "#ffffff";
            tokens["@text"] = "#000000";
            tokens["@textStrong"] = "#000000";
        }
    } else {
        // 背景、面板、边框等中性色混入少量种子色，整体氛围随主题色一起变化
        for (auto it = tokens.begin(); it != tokens.end(); ++it) {
            const double ratio = tintRatio(it.key(), dark);
            if (ratio > 0) it.value() = mix(QColor(it.value()), seedColor, ratio).name();
        }
    }

    if (custom.contains("window")) {
        const QColor background = custom.value("window");
        const QColor foreground = contrastingText(background);
        tokens["@window"] = background.name();
        const QHash<QString, double> ratios = {
                {"@surface", 0.02}, {"@panel", 0.05}, {"@panelAlt", 0.03}, {"@disabledBg", 0.04},
                {"@border", 0.16}, {"@borderLight", 0.10}, {"@inputBorder", 0.20}, {"@cardBorder", 0.16},
                {"@hover", 0.08}, {"@hoverBorder", 0.25}, {"@hoverSoft", 0.05}, {"@tabHover", 0.08},
                {"@scrollbar", 0.20}, {"@scrollbarHover", 0.35}, {"@inputHoverBorder", 0.30}, {"@checkBorder", 0.30}};
        for (auto it = ratios.cbegin(); it != ratios.cend(); ++it) tokens[it.key()] = mix(background, foreground, it.value()).name();
    }
    if (custom.contains("window") || custom.contains("text")) {
        const QColor background(tokens.value("@window"));
        const QColor foreground = custom.value("text", contrastingText(background));
        tokens["@text"] = foreground.name();
        tokens["@textStrong"] = foreground.name();
        tokens["@textSecondary"] = mix(foreground, background, 0.20).name();
        tokens["@textDim"] = mix(foreground, background, 0.35).name();
        tokens["@textDisabled"] = mix(foreground, background, 0.60).name();
        tokens["@textFaint"] = mix(foreground, background, 0.75).name();
    }

    tokens["@accent"] = accentColor.name();
    tokens["@accentStrong"] = dark ? accentColor.name()
                                   : accent.textLight ? QString(QLatin1String(accent.textLight))
                                                      : accentColor.darker(120).name();
    tokens["@accentPressed"] = accentColor.darker(dark ? 115 : 130).name();
    // 强调色的浅洗色按固定比例混入表面色派生，避免为每个主题色手写一套
    const QColor surface(tokens.value("@surface"));
    tokens["@accentFaint"] = mix(surface, accentColor, dark ? 0.14 : 0.08).name();
    tokens["@accentWash"] = mix(surface, accentColor, dark ? 0.18 : 0.10).name();
    tokens["@accentSoft"] = mix(surface, accentColor, dark ? 0.22 : 0.12).name();
    tokens["@accentMuted"] = mix(surface, accentColor, dark ? 0.28 : 0.16).name();
    tokens["@accentBorder"] = mix(surface, accentColor, dark ? 0.55 : 0.45).name();
    tokens["@onAccent"] = dark ? QString("#0f1114") : accent.onLight ? QString(QLatin1String(accent.onLight))
                                                                     : QString("#ffffff");
    if (id == "custom") {
        tokens["@onAccent"] = contrastingText(accentColor).name();
    }
    tokens["@focus"] = custom.value("focus", accentColor).name();
    tokens["@focusBorder"] = custom.value("focus", QColor(tokens.value("@accentBorder"))).name();
    tokens["@focusStrong"] = custom.value("focus", QColor(tokens.value("@text"))).name();

    return tokens;
}

}// namespace

QColor customColor(const QString &token, const QColor &seed, const QHash<QString, QColor> &overrides, bool dark) {
    return {buildTokens(dark, "custom", seed, overrides).value("@" + token)};
}

QColor color(const QString &token) {
    return color(token, accentId(), isDark());
}

QColor color(const QString &token, const QString &id, bool dark) {
    return {buildTokens(dark, id).value("@" + token)};
}

QString process(const QString &stylesheet) {
    const auto tokens = buildTokens(isDark(), accentId());
    auto keys = tokens.keys();
    std::sort(keys.begin(), keys.end(), [](const QString &a, const QString &b) { return a.length() > b.length(); });
    QString result = stylesheet;
    for (const auto &key: keys) result.replace(key, tokens.value(key));
    return result;
}

}// namespace Theme
