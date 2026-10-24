#pragma once

#include "OverlayDialog.h"

struct Notebook;
class SitePreviewServer;
class QWebEngineView;

class SitePreviewDialog : public OverlayDialog {
public:
    SitePreviewDialog(const Notebook &book, QWidget *parent);
    bool start(QString *error);

private:
    SitePreviewServer *server_;
    QWebEngineView *browser_;
    QString root_;
};
