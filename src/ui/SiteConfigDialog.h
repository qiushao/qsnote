#pragma once

#include "OverlayDialog.h"

struct Notebook;

class SiteConfigDialog : public OverlayDialog {
    Q_OBJECT
public:
    // Edits the supplied notebook immediately, matching the existing settings workflow.
    explicit SiteConfigDialog(Notebook &book, QWidget *parent);

signals:
    void saveRequested(bool delayed);
    void categoriesChanged();
};
