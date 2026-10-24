#include "OverlayDialog.h"
#include <QPainterPath>

OverlayDialog::OverlayDialog(QWidget *parent, const QString &name, bool wide) : QDialog(parent), scrim_(new QWidget(parent)) {
    setObjectName(name);
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    if (name == "settingsDialog") {
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_StyledBackground);
    }
    setModal(true);
    scrim_->setStyleSheet("background: rgba(0,0,0,77);");
    scrim_->setGeometry(parent->rect());
    scrim_->show();
    scrim_->raise();
    const int preferred = wide ? 660 : 380;
    resize(qMin(preferred, parent->width() * 92 / 100), wide ? parent->height() * 80 / 100 : qMin(280, parent->height() * 80 / 100));
    setMaximumSize(parent->size() * 0.92);
}

OverlayDialog::~OverlayDialog() { delete scrim_; }

void OverlayDialog::resizeEvent(QResizeEvent *event) {
    QDialog::resizeEvent(event);
    if (testAttribute(Qt::WA_TranslucentBackground)) return;
    QPainterPath shape;
    shape.addRoundedRect(QRectF(rect()), 4, 4);
    setMask(QRegion(shape.toFillPolygon().toPolygon()));
}

void OverlayDialog::showEvent(QShowEvent *event) {
    QDialog::showEvent(event);
    move(parentWidget()->mapToGlobal(parentWidget()->rect().center()) - rect().center());
}
