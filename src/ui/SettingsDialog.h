#pragma once

#include "OverlayDialog.h"
#include <QColor>
#include <QHash>
#include <array>
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
    QColor customSeed_;
    std::array<QHash<QString, QColor>, 2> customOverrides_;
    std::function<void()> apply_;
};
