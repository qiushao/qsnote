#pragma once

#include "OverlayDialog.h"
#include <functional>

class QLabel;
class MarkdownEditor;

class SettingsDialog : public OverlayDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(MarkdownEditor *editor, QWidget *parent);
    void accept() override;

signals:
    void updateCheckRequested(QLabel *status);

private:
    std::function<void()> apply_;
};
