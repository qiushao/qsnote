#ifndef QSNOTE_SITEPAGE_H
#define QSNOTE_SITEPAGE_H

#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

struct SitePage {
    QString id;
    QString collectionId;
    bool document = false;
    QString source;
    QString route;
    QString title;
    QString body;
    QString html;
    QString excerpt;
    QString toc;
    QString text;
    QString date;
    QString updated;
    QStringList categories;
    QStringList tags;
    QString keywords;
    bool post = false;
    bool pinned = false;
    bool comments = true;
};

struct SiteIndex {
    QList<const SitePage *> posts;
    QMap<QString, const SitePage *> byId;
    QMap<QString, QList<const SitePage *>> documents;
    QMap<QString, QList<const SitePage *>> categories, tags, archives;
};

#endif
