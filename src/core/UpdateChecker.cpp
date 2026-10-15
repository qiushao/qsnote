#include "UpdateChecker.h"
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QVersionNumber>

UpdateChecker::Result UpdateChecker::parseRelease(const QByteArray &data, const QString &currentVersion) {
    const auto release = QJsonDocument::fromJson(data).object();
    const auto tag = release.value("tag_name").toString();
    const QRegularExpression pattern(R"(^[vV]?(\d+\.\d+\.\d+)$)");
    const auto latest = pattern.match(tag);
    const auto current = pattern.match(currentVersion);
    if (!latest.hasMatch() || !current.hasMatch() || !release.value("draft").isBool() || !release.value("prerelease").isBool()) {
        return {{}, QCoreApplication::translate("UpdateChecker", "无法识别版本信息，请前往项目发布页查看。")};
    }
    if (release.value("draft").toBool() || release.value("prerelease").toBool()) return {};
    if (QVersionNumber::fromString(latest.captured(1)) > QVersionNumber::fromString(current.captured(1))) return {tag, {}};
    return {};
}

void UpdateChecker::check() {
    if (checking_) return;
    checking_ = true;
    auto *network = new QNetworkAccessManager(this);
    QNetworkRequest request(QUrl("https://api.github.com/repos/qiushao/qsnote/releases/latest"));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("User-Agent", "qsnote/" + QCoreApplication::applicationVersion().toUtf8());
    request.setTransferTimeout(15000);
    auto *reply = network->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, network, reply] {
        Result result;
        if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 404) {
            result.error = QCoreApplication::translate("UpdateChecker", "暂未找到已发布的版本。");
        } else if (reply->error() != QNetworkReply::NoError) {
            result.error = QCoreApplication::translate("UpdateChecker", "检查更新失败：") + reply->errorString();
        } else {
            result = parseRelease(reply->readAll(), QCoreApplication::applicationVersion());
        }
        checking_ = false;
        network->deleteLater();
        emit finished(result.version, result.error);
    });
}
