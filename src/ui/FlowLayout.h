#ifndef QSNOTE_FLOWLAYOUT_H
#define QSNOTE_FLOWLAYOUT_H

#include <QLayout>

// Wrapping rows for the Markdown tools and post metadata.
class FlowLayout : public QLayout {
public:
    explicit FlowLayout(QWidget *parent, int margin, int spacing);
    ~FlowLayout() override;
    void addItem(QLayoutItem *item) override;
    int count() const override;
    QLayoutItem *itemAt(int index) const override;
    QLayoutItem *takeAt(int index) override;
    Qt::Orientations expandingDirections() const override;
    bool hasHeightForWidth() const override;
    int heightForWidth(int width) const override;
    QSize sizeHint() const override;
    QSize minimumSize() const override;
    void setGeometry(const QRect &rect) override;

private:
    int arrange(const QRect &rect, bool apply) const;
    QList<QLayoutItem *> items_;
};

#endif// QSNOTE_FLOWLAYOUT_H
