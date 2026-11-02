#pragma once

#include "OverlayDialog.h"
#include "core/NotebookStore.h"

class SiteConfigDialog : public OverlayDialog {
    Q_OBJECT
public:
    // 编辑笔记本的副本，点击“保存”后才写回笔记本。
    explicit SiteConfigDialog(Notebook &book, QWidget *parent);
    void accept() override;

signals:
    void categoriesChanged();

private:
    Notebook &notebook_;
    Notebook draft_;
    QString pendingAvatar_;
    bool categoriesDirty_ = false;
};
