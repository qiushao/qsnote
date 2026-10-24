#ifndef QSNOTE_SITETEMPLATERENDERER_H
#define QSNOTE_SITETEMPLATERENDERER_H

#include "NotebookStore.h"
#include <QMap>

class SiteTemplateRenderer {
public:
    enum class Theme { Pure,
                       Next,
                       Yilia,
                       Three,
                       Custom };
    explicit SiteTemplateRenderer(const Notebook &book);
    bool load(QString *error);
    QString render(const QString &name, const QMap<QString, QString> &values) const;
    Theme theme() const { return theme_; }
    bool contains(const QString &name) const { return customTemplates_.contains(name); }
    QString assetsPath() const;

private:
    QString themeName() const;
    const Notebook &book_;
    Theme theme_;
    QString customThemePath_;
    QMap<QString, QString> customTemplates_;
};

#endif
