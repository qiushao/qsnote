#include "ApplicationLanguage.h"
#include <QCoreApplication>
#include <QDebug>
#include <QSettings>

QString ApplicationLanguage::resolve(const QString &preference, const QLocale &systemLocale) {
    if (preference == "zh_CN" || preference == "en_US") return preference;
    return systemLocale.language() == QLocale::Chinese ? QStringLiteral("zh_CN") : QStringLiteral("en_US");
}

ApplicationLanguage::ApplicationLanguage() {
    QSettings settings;
    // INI files normalize the reserved General group to this capitalization.
    // Preserve lowercase keys from native backends that retain their spelling.
    for (const auto &key: settings.allKeys()) {
        if (!key.startsWith("general/")) continue;
        const auto canonical = "General/" + key.mid(8);
        if (!settings.contains(canonical)) settings.setValue(canonical, settings.value(key));
    }
    settings.sync();
    const auto preference = settings.value("General/language", "system").toString();
    const auto language = resolve(preference);
    QLocale::setDefault(preference == "zh_CN" || preference == "en_US" ? QLocale(language) : QLocale::system());
    auto install = [](QTranslator &translator, const QString &path) {
        if (translator.load(path)) QCoreApplication::installTranslator(&translator);
        else qWarning() << "Unable to load translation:" << path;
    };
    if (language == "en_US") {
        install(application_, ":/translations/qsnote_en_US.qm");
    } else {
        install(qt_, ":/translations/qtbase_zh_CN.qm");
        install(editor_, ":/translations/vtextedit_zh_CN.qm");
    }
}
