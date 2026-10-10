#include "FlowLayout.h"
#include <QWidget>
#include <QVariant>

FlowLayout::FlowLayout(QWidget *parent, int margin, int spacing) : QLayout(parent) {
    setContentsMargins(margin, margin, margin, margin);
    setSpacing(spacing);
}

FlowLayout::~FlowLayout() {
    while (auto *item = takeAt(0)) delete item;
}

void FlowLayout::addItem(QLayoutItem *item) { items_.append(item); }
int FlowLayout::count() const { return static_cast<int>(items_.size()); }
QLayoutItem *FlowLayout::itemAt(int index) const { return items_.value(index); }
QLayoutItem *FlowLayout::takeAt(int index) { return index >= 0 && index < count() ? items_.takeAt(index) : nullptr; }
Qt::Orientations FlowLayout::expandingDirections() const { return {}; }
bool FlowLayout::hasHeightForWidth() const { return true; }
int FlowLayout::heightForWidth(int width) const { return arrange(QRect(0, 0, width, 0), false); }
QSize FlowLayout::sizeHint() const { return minimumSize(); }
QSize FlowLayout::minimumSize() const {
    QSize size;
    for (auto *item: items_) size = size.expandedTo(item->minimumSize());
    const auto margins = contentsMargins();
    return size + QSize(margins.left() + margins.right(), margins.top() + margins.bottom());
}
void FlowLayout::setGeometry(const QRect &rect) {
    QLayout::setGeometry(rect);
    arrange(rect, true);
}
int FlowLayout::arrange(const QRect &rect, bool apply) const {
    const auto margins = contentsMargins();
    const auto area = rect.marginsRemoved(margins);
    int x = area.x();
    int y = area.y();
    int rowHeight = 0;
    for (auto *item: items_) {
        if (item->isEmpty()) continue;
        QSize size = item->sizeHint();
        size.setWidth(qMin(size.width(), area.width()));
        if (x > area.x() && x + size.width() > area.right() + 1) {
            x = area.x();
            y += rowHeight + spacing();
            rowHeight = 0;
        }
        if (item->widget() && item->widget()->property("alignRight").toBool()) x = qMax(x, area.right() + 1 - size.width());
        if (apply) item->setGeometry(QRect(QPoint(x, y), size));
        x += size.width() + spacing();
        rowHeight = qMax(rowHeight, size.height());
    }
    return y + rowHeight - rect.y() + margins.bottom();
}
