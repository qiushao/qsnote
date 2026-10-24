#pragma once

#include <QCoreApplication>
#include <QDialog>
#include <QJsonObject>

class QLineEdit;
class QComboBox;
class QDateEdit;
class QCheckBox;

class PostPropertiesDialog : public QDialog {
    Q_DECLARE_TR_FUNCTIONS(MainWindow)
public:
    PostPropertiesDialog(const QString &title, const QJsonObject &metadata, const QStringList &categories,
                         bool editing, QWidget *parent = nullptr);
    QString title() const;
    QJsonObject metadata() const;

private:
    QJsonObject initialMetadata_;
    QLineEdit *title_;
    QComboBox *category_;
    QLineEdit *tags_;
    QLineEdit *keywords_ = nullptr;
    QDateEdit *date_;
    QLineEdit *slug_;
    QCheckBox *draft_;
};
