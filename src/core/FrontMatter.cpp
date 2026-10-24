#include "FrontMatter.h"
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>

// QSNote writes JSON front matter. Accept the simple YAML fields used when authoring pages too.
bool FrontMatter::parse(QString &body, QJsonObject &metadata, QString *error) {
    body.replace("\r\n", "\n");
    if (!body.startsWith("---\n")) return true;
    const auto end = body.indexOf("\n---", 4);
    if (end < 0 || (end + 4 < body.size() && body[end + 4] != '\n')) {
        *error = QCoreApplication::translate("SiteGenerator", "front matter 缺少结束分隔符");
        return false;
    }
    const auto header = body.mid(4, end - 4).trimmed();
    if (header.startsWith('{')) {
        QJsonParseError parseError;
        const auto document = QJsonDocument::fromJson(header.toUtf8(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            *error = QCoreApplication::translate("SiteGenerator", "front matter JSON 无效：") + parseError.errorString();
            return false;
        }
        metadata = document.object();
    } else {
        QString listKey;
        const auto scalar = [](QString value) -> QJsonValue {
            value = value.trimmed();
            if (value == "true") return true;
            if (value == "false") return false;
            if (value.startsWith('"')) {
                const auto parsed = QJsonDocument::fromJson(("[" + value + "]").toUtf8());
                if (parsed.isArray()) return parsed.array().first();
            }
            if (value.startsWith('\'') && value.endsWith('\'')) return value.mid(1, value.size() - 2).replace("''", "'");
            return value;
        };
        for (const auto &line: header.split('\n')) {
            const auto trimmed = line.trimmed();
            if (trimmed.isEmpty() || trimmed.startsWith('#')) continue;
            if (trimmed.startsWith("- ") && !listKey.isEmpty()) {
                auto array = metadata[listKey].toArray();
                array.append(scalar(trimmed.mid(2)));
                metadata[listKey] = array;
                continue;
            }
            const auto match = QRegularExpression("^([A-Za-z_][A-Za-z0-9_]*):(?:\\s+(.*))?$").match(line);
            if (!match.hasMatch()) {
                *error = QCoreApplication::translate("SiteGenerator", "不支持的 front matter 行，请使用 JSON 或简单 YAML 字段：") + line;
                return false;
            }
            const auto key = match.captured(1);
            const auto value = match.captured(2).trimmed();
            listKey = value.isEmpty() ? key : QString();
            if (value.startsWith('[') && value.endsWith(']')) {
                QJsonArray array;
                for (const auto &item: value.mid(1, value.size() - 2).split(',', Qt::SkipEmptyParts)) array.append(scalar(item));
                metadata[key] = array;
            } else
                metadata[key] = scalar(value);
        }
    }
    body = body.mid(qMin(end + 5, body.size()));
    return true;
}
