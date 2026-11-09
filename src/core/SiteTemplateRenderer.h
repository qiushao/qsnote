#ifndef QSNOTE_SITETEMPLATERENDERER_H
#define QSNOTE_SITETEMPLATERENDERER_H

#include "NotebookStore.h"
#include <QMap>

class SiteTemplateRenderer {
public:
    explicit SiteTemplateRenderer(const Notebook &book);
    bool load(QString *error);
    QString render(const QString &name, const QJsonObject &context) const;
    QString renderEntry(const QString &type, const QJsonObject &context) const;
    QString assetsPath() const;
    QString directory() const { return themePath_; }
    QJsonObject options() const { return options_; }

private:
    struct Node {
        enum class Kind { Text, Value, Raw, Section, Inverted, Partial };
        Kind kind = Kind::Text;
        QString value;
        QString dateFormat;
        QList<Node> children;
        QString dateLocale;
    };
    bool parse(const QString &source, qsizetype &offset, QList<Node> &nodes, const QString &block, QString *error) const;
    QString evaluate(const QList<Node> &nodes, const QList<QJsonValue> &scopes) const;
    const Notebook &book_;
    QString themePath_;
    QString assetsDirectory_;
    QJsonObject entries_;
    QJsonObject options_;
    QMap<QString, QList<Node>> templates_;
};

#endif
