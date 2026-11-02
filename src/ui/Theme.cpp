#include "Theme.h"

#include <QGuiApplication>
#include <QHash>
#include <QSettings>
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

// 「四时山水」主题色，取自 Gridea Pro tokens.css
constexpr AccentDef kAccents[] = {
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
    return kAccents[0];// green
}

QColor mix(const QColor &base, const QColor &tint, double t) {
    const auto f = static_cast<float>(t);
    return QColor::fromRgbF(base.redF() + (tint.redF() - base.redF()) * f,
                            base.greenF() + (tint.greenF() - base.greenF()) * f,
                            base.blueF() + (tint.blueF() - base.blueF()) * f);
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
    return ids;
}

QString accentId() {
    return accentDef(QSettings().value("Appearance/accent", "green").toString()).id;
}

QColor accentColor(const QString &id, bool dark) {
    const auto &def = accentDef(id);
    return {QLatin1String(dark ? def.dark : def.light)};
}

QColor accentColor() {
    return accentColor(accentId(), isDark());
}

QString process(const QString &stylesheet) {
    const bool dark = isDark();
    const auto &accent = accentDef(accentId());
    const QColor accentColor(QLatin1String(dark ? accent.dark : accent.light));
    const QColor surface(dark ? "#242428" : "#ffffff");

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

    tokens["@accent"] = accentColor.name();
    tokens["@accentStrong"] = dark ? accentColor.name()
                                   : accent.textLight ? QString(QLatin1String(accent.textLight))
                                                      : accentColor.darker(120).name();
    tokens["@accentPressed"] = accentColor.darker(dark ? 115 : 130).name();
    // 强调色的浅洗色按固定比例混入表面色派生，避免为每个主题色手写一套
    tokens["@accentFaint"] = mix(surface, accentColor, dark ? 0.14 : 0.08).name();
    tokens["@accentWash"] = mix(surface, accentColor, dark ? 0.18 : 0.10).name();
    tokens["@accentSoft"] = mix(surface, accentColor, dark ? 0.22 : 0.12).name();
    tokens["@accentMuted"] = mix(surface, accentColor, dark ? 0.28 : 0.16).name();
    tokens["@accentBorder"] = mix(surface, accentColor, dark ? 0.55 : 0.45).name();
    tokens["@onAccent"] = dark ? QString("#0f1114") : accent.onLight ? QString(QLatin1String(accent.onLight))
                                                                     : QString("#ffffff");

    auto keys = tokens.keys();
    std::sort(keys.begin(), keys.end(), [](const QString &a, const QString &b) { return a.length() > b.length(); });
    QString result = stylesheet;
    for (const auto &key: keys) result.replace(key, tokens.value(key));
    return result;
}

}// namespace Theme
