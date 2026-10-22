#pragma once

#include <QPushButton>
#include <QStyle>
#include <QVariant>

inline void setPrimaryButton(QPushButton *button) {
    button->setProperty("primary", true);
    // QDialogButtonBox may polish its buttons before the role is assigned.
    button->style()->unpolish(button);
    button->style()->polish(button);
}
