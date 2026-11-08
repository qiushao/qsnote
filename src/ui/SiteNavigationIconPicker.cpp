#include "SiteNavigationIconPicker.h"
#include "UiStyle.h"
#include "core/SiteNavigationIcons.h"
#include <QComboBox>
#include <QLineEdit>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QWebEngineSettings>
#include <QWebEngineView>
#include <memory>

SiteNavigationIconPicker::SiteNavigationIconPicker(const QJsonObject &item, QWidget *parent) : QWidget(parent) {
    auto *layout = column(this, 0, 8);
    choice_ = new QComboBox(this);
    choice_->setObjectName("navigationIcon");
    choice_->addItem(tr("无图标"), QString());
    for (const auto &entry: SiteNavigationIcons::items())
        choice_->addItem(QIcon(SiteNavigationIcons::resource(QLatin1String(entry.id))), QCoreApplication::translate("SiteNavigationIcons", entry.label), QString::fromLatin1(entry.id));
    choice_->addItem("Iconify", "iconify");
    layout->addWidget(choice_);
    details_ = new QWidget(this);
    detailsLayout_ = column(details_, 0, 8);
    auto *entry = new QWidget(details_);
    auto *entryLayout = row(entry, 0, 0, 8);
    name_ = new QLineEdit(entry);
    name_->setObjectName("navigationIconifyName");
    name_->setPlaceholderText("line-md:coffee-loop");
    name_->setAccessibleName(tr("Iconify 图标名称"));
    download_ = new QPushButton(tr("下载并预览"), entry);
    download_->setObjectName("downloadNavigationIcon");
    entryLayout->addWidget(name_, 1);
    entryLayout->addWidget(download_);
    detailsLayout_->addWidget(entry);
    auto *help = label(tr("填写“图标集:名称”，例如 map:wind-surfing。<a href=\"https://icon-sets.iconify.design/\">浏览 Iconify 图标库</a>"), details_, true);
    help->setOpenExternalLinks(true);
    detailsLayout_->addWidget(help);
    status_ = label({}, details_, true);
    status_->setObjectName("navigationIconStatus");
    status_->setTextFormat(Qt::PlainText);
    detailsLayout_->addWidget(status_);
    layout->addWidget(details_);
    network_ = new QNetworkAccessManager(this);
    const auto id = item["icon"].toString();
    if (id.contains(':')) {
        choice_->setCurrentIndex(choice_->findData("iconify"));
        name_->setText(id);
        svg_ = SiteNavigationIcons::normalizeSvg(item["iconSvg"].toString().toUtf8());
        if (!svg_.isEmpty()) loadedName_ = id;
    } else choice_->setCurrentIndex(qMax(0, choice_->findData(id)));
    connect(choice_, &QComboBox::currentIndexChanged, this, &SiteNavigationIconPicker::updateState);
    connect(name_, &QLineEdit::textChanged, this, &SiteNavigationIconPicker::updateState);
    connect(download_, &QPushButton::clicked, this, &SiteNavigationIconPicker::download);
    updateState();
}

bool SiteNavigationIconPicker::isValid() const {
    return choice_->currentData().toString() != "iconify" ||
        (SiteNavigationIcons::isIconifyName(name_->text().trimmed()) && loadedName_ == name_->text().trimmed() && !svg_.isEmpty());
}

void SiteNavigationIconPicker::apply(QJsonObject &item) const {
    item.remove("icon");
    item.remove("iconSvg");
    const auto choice = choice_->currentData().toString();
    if (choice == "iconify" && isValid()) {
        item["icon"] = loadedName_;
        item["iconSvg"] = svg_;
    } else if (!choice.isEmpty() && choice != "iconify") item["icon"] = choice;
}

void SiteNavigationIconPicker::updateState() {
    const bool iconify = choice_->currentData().toString() == "iconify";
    details_->setVisible(iconify);
    name_->setEnabled(!loading_);
    download_->setEnabled(!loading_ && SiteNavigationIcons::isIconifyName(name_->text().trimmed()));
    if (loading_) status_->setText(tr("正在下载图标…"));
    else if (iconify && isValid()) status_->setText(tr("图标已缓存，保存后可离线生成站点。"));
    else status_->setText(tr("请输入有效名称，然后下载并预览。"));
    if (iconify && isValid()) showPreview();
    else if (preview_) preview_->hide();
    emit validityChanged();
}

void SiteNavigationIconPicker::showPreview() {
    if (!preview_) {
        preview_ = new QWebEngineView(details_);
        preview_->setObjectName("navigationIconPreview");
        preview_->setFixedHeight(80);
        preview_->settings()->setAttribute(QWebEngineSettings::JavascriptEnabled, false);
        preview_->setContextMenuPolicy(Qt::NoContextMenu);
        detailsLayout_->addWidget(preview_);
    }
    const auto icon = SiteNavigationIcons::markup({{"icon", loadedName_}, {"iconSvg", svg_}}, "preview-");
    const auto colors = palette();
    preview_->setHtml("<!doctype html><html><head><meta charset=\"utf-8\"><style>body{margin:0;display:flex;align-items:center;justify-content:center;height:80px;color:" + colors.color(QPalette::Text).name() +
        ";background:" + colors.color(QPalette::Window).name() + ";}svg{width:48px;height:48px}</style></head><body>" + icon + "</body></html>");
    preview_->show();
}

void SiteNavigationIconPicker::download() {
    const auto id = name_->text().trimmed();
    if (loading_ || !SiteNavigationIcons::isIconifyName(id)) return;
    loading_ = true;
    updateState();
    auto path = id;
    path.replace(':', '/');
    QNetworkRequest request(QUrl("https://api.iconify.design/" + path + ".svg"));
    request.setTransferTimeout(15000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::SameOriginRedirectPolicy);
    request.setRawHeader("Accept", "image/svg+xml");
    auto *reply = network_->get(request);
    const auto bytes = std::make_shared<QByteArray>();
    connect(reply, &QNetworkReply::readyRead, this, [reply, bytes] {
        *bytes += reply->readAll();
        if (bytes->size() > SiteNavigationIcons::maximumSvgBytes) reply->abort();
    });
    QTimer::singleShot(15000, reply, [reply] { if (reply->isRunning()) reply->abort(); });
    connect(reply, &QNetworkReply::finished, this, [this, reply, bytes, id] {
        *bytes += reply->readAll();
        QString error;
        if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 404) error = tr("找不到该 Iconify 图标，请检查名称。");
        else if (reply->error() != QNetworkReply::NoError) error = tr("无法下载图标：%1").arg(reply->errorString());
        else {
            const auto svg = SiteNavigationIcons::normalizeSvg(*bytes, &error);
            if (!svg.isEmpty()) { loadedName_ = id; svg_ = svg; }
        }
        loading_ = false;
        updateState();
        if (!error.isEmpty()) status_->setText(error);
        reply->deleteLater();
    });
}
