#pragma once

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>
#include <QVariant>

inline void setPrimaryButton(QPushButton *button) {
    button->setProperty("primary", true);
    // QDialogButtonBox may polish its buttons before the role is assigned.
    button->style()->unpolish(button);
    button->style()->polish(button);
}

inline QVBoxLayout *column(QWidget *widget, int margin = 0, int spacing = 0) {
    auto *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(margin, margin, margin, margin);
    layout->setSpacing(spacing);
    return layout;
}
inline QHBoxLayout *row(QWidget *widget, int horizontal = 10, int vertical = 8, int spacing = 6) {
    auto *layout = new QHBoxLayout(widget);
    layout->setContentsMargins(horizontal, vertical, horizontal, vertical);
    layout->setSpacing(spacing);
    return layout;
}
inline QLabel *label(const QString &text, QWidget *parent, bool dim = false) {
    auto *result = new QLabel(text, parent);
    result->setProperty("dim", dim);
    result->setWordWrap(true);
    return result;
}
inline QPushButton *button(const QString &text, const QString &name, QWidget *parent) {
    auto *result = new QPushButton(text, parent);
    result->setObjectName(name);
    result->setCursor(Qt::PointingHandCursor);
    return result;
}
