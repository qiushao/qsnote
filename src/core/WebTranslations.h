#ifndef QSNOTE_WEBTRANSLATIONS_H
#define QSNOTE_WEBTRANSLATIONS_H

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QWebEnginePage>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>

inline void installWebTranslations(QWebEnginePage *page) {
    const QJsonObject messages{
        {"language", QCoreApplication::translate("Web", "zh-CN")},
        {"preview", QCoreApplication::translate("Web", "Markdown 预览")},
        {"metadata", QCoreApplication::translate("Web", "文档元数据")},
        {"plantUml", QCoreApplication::translate("Web", "PlantUML 图表")},
        {"plantUmlImageError", QCoreApplication::translate("Web", "无法读取 PlantUML 图片")},
        {"diagram", QCoreApplication::translate("Web", "图表")},
        {"diagramError", QCoreApplication::translate("Web", "图表渲染失败：")},
        {"mathError", QCoreApplication::translate("Web", "公式 / Markdown：")},
        {"mathFonts", QCoreApplication::translate("Web", "公式字体")},
        {"imageSizeError", QCoreApplication::translate("Web", "图片尺寸无效")},
        {"image", QCoreApplication::translate("Web", "图片：")},
        {"imageConversionError", QCoreApplication::translate("Web", "无法转换图片")},
        {"previewImageError", QCoreApplication::translate("Web", "无法读取预览图片")},
        {"missingDiagram", QCoreApplication::translate("Web", "未生成图表")},
        {"diagramSizeError", QCoreApplication::translate("Web", "图表尺寸无效")}
    };
    QWebEngineScript script;
    script.setName("qsnote-translations");
    script.setInjectionPoint(QWebEngineScript::DocumentCreation);
    script.setWorldId(QWebEngineScript::MainWorld);
    script.setSourceCode("window.qsnoteTranslations = " + QString::fromUtf8(QJsonDocument(messages).toJson(QJsonDocument::Compact)) + ";");
    page->scripts().insert(script);
}

#endif // QSNOTE_WEBTRANSLATIONS_H
