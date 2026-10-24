#pragma once

#include <QDialog>

// Parent-owned scrim keeps dialogs visually inside the application.
class OverlayDialog : public QDialog {
public:
    OverlayDialog(QWidget *parent, const QString &name, bool wide = false);
    ~OverlayDialog() override;

protected:
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    QWidget *scrim_;
};
