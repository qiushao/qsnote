#ifndef QSNOTE_FRONTMATTER_H
#define QSNOTE_FRONTMATTER_H

#include <QJsonObject>
#include <QString>

namespace FrontMatter {
// Parses author JSON/simple YAML metadata and removes its header from the body.
// This is separate from StaticSite::postBody, which strips QSNote's generated header.
bool parse(QString &body, QJsonObject &metadata, QString *error);
}// namespace FrontMatter

#endif
