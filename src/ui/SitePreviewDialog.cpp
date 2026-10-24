#include "SitePreviewDialog.h"
#include "UiStyle.h"
#include "core/NotebookStore.h"
#include "core/SitePreviewServer.h"
#include "core/StaticSite.h"
#include <QCoreApplication>
#include <QHostAddress>
#include <QUrl>
#include <QWebEngineView>

SitePreviewDialog::SitePreviewDialog(const Notebook &book, QWidget *parent)
    : OverlayDialog(parent, "sitePreviewDialog", true),
      server_(new SitePreviewServer(book.path + "/public", StaticSite::rootPath(book), this)),
      root_(StaticSite::rootPath(book)) {
    setMaximumSize(parent->size());
    resize(parent->size());
    auto *layout = column(this);
    auto *bar = new QWidget(this);
    bar->setObjectName("dialogBar");
    auto *barLayout = row(bar, 16, 11);
    auto *title = label(QCoreApplication::translate("MainWindow", "站点预览"), bar);
    title->setProperty("dialogTitle", true);
    barLayout->addWidget(title);
    barLayout->addWidget(label(book.config["theme"].toString() == "custom" ? QCoreApplication::translate("MainWindow", "自定义") : book.config["theme"].toString() == "next" ? "NexT"
                                                                                                                           : book.config["theme"].toString() == "yilia"      ? "Yilia"
                                                                                                                           : book.config["theme"].toString() == "3-jekyll"   ? "3-Jekyll"
                                                                                                                                                                             : "Pure",
                               bar, true));
    barLayout->addStretch();
    auto *close = button(QCoreApplication::translate("MainWindow", "关闭"), "closeSitePreview", bar);
    barLayout->addWidget(close);
    layout->addWidget(bar);
    browser_ = new QWebEngineView(this);
    browser_->setObjectName("siteBrowser");
    layout->addWidget(browser_, 1);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
}

bool SitePreviewDialog::start(QString *error) {
    if (!server_->listen(QHostAddress::LocalHost)) {
        *error = server_->errorString();
        return false;
    }
    QUrl url;
    url.setScheme("http");
    url.setHost("127.0.0.1");
    url.setPort(server_->serverPort());
    url.setPath(root_);
    browser_->load(url);
    return true;
}
