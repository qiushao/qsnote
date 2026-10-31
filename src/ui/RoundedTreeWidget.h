#pragma once

#include <QPainter>
#include <QPainterPath>
#include <QTreeWidget>

class RoundedTreeWidget : public QTreeWidget {
public:
    using QTreeWidget::QTreeWidget;

protected:
    void drawRow(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        // Qt paints branch and item backgrounds separately. Round only the whole row.
        QPainterPath clip;
        clip.addRoundedRect(QRectF(option.rect), 6, 6);
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setClipPath(clip, Qt::IntersectClip);
        QTreeWidget::drawRow(painter, option, index);
        painter->restore();
    }
};
