#pragma once

#include <QJsonObject>
#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QNetworkAccessManager;
class QPushButton;
class QVBoxLayout;
class QWebEngineView;

class SiteNavigationIconPicker : public QWidget {
    Q_OBJECT
public:
    explicit SiteNavigationIconPicker(const QJsonObject &item, QWidget *parent);
    bool isValid() const;
    void apply(QJsonObject &item) const;

signals:
    void validityChanged();

private:
    void updateState();
    void download();
    void showPreview();
    QComboBox *choice_;
    QWidget *details_;
    QLineEdit *name_;
    QPushButton *download_;
    QLabel *status_;
    QVBoxLayout *detailsLayout_;
    QWebEngineView *preview_ = nullptr;
    QNetworkAccessManager *network_;
    QString loadedName_, svg_;
    bool loading_ = false;
};
