#include "PostPropertiesDialog.h"
#include "UiStyle.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QScreen>

PostPropertiesDialog::PostPropertiesDialog(const QString &title, const QJsonObject &metadata, const QStringList &categories,
                                           bool editing, QWidget *parent)
    : QDialog(parent), initialMetadata_(metadata) {
    setWindowTitle(editing ? tr("文章属性") : tr("新建文章"));
    auto *layout = new QFormLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(12);
    layout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    layout->setRowWrapPolicy(QFormLayout::WrapLongRows);
    title_ = new QLineEdit(title, this);
    layout->addRow(tr("名称："), title_);
    category_ = new QComboBox(this);
    category_->addItems(categories);
    category_->setCurrentIndex(qMax(0, category_->findText(metadata["category"].toString())));
    layout->addRow(tr("分类："), category_);
    tags_ = new QLineEdit(metadata["tags"].toString(), this);
    tags_->setPlaceholderText(tr("逗号分隔"));
    layout->addRow(tr("标签："), tags_);
    if (editing) {
        keywords_ = new QLineEdit(metadata["keywords"].toString(), this);
        keywords_->setObjectName("postKeywords");
        keywords_->setPlaceholderText(tr("逗号分隔；留空使用站点关键词"));
        layout->addRow(tr("SEO 关键词："), keywords_);
    }
    date_ = new QDateEdit(QDate::fromString(metadata["date"].toString(), Qt::ISODate), this);
    date_->setDisplayFormat("yyyy-MM-dd");
    date_->setCalendarPopup(true);
    layout->addRow(tr("日期："), date_);
    slug_ = new QLineEdit(metadata["slug"].toString(), this);
    layout->addRow("Slug：", slug_);
    draft_ = new QCheckBox(tr("保存为草稿"), this);
    draft_->setChecked(metadata["draft"].toBool());
    layout->addRow(draft_);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    setPrimaryButton(buttons->button(QDialogButtonBox::Ok));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addRow(buttons);
    if (!editing) resize(qMin(560, screen()->availableGeometry().width() * 92 / 100), sizeHint().height());
}

QString PostPropertiesDialog::title() const { return title_->text().trimmed(); }

QJsonObject PostPropertiesDialog::metadata() const {
    auto result = initialMetadata_;
    result["category"] = category_->currentText();
    result["tags"] = tags_->text().trimmed();
    if (keywords_) result["keywords"] = keywords_->text().trimmed();
    result["date"] = date_->date().toString(Qt::ISODate);
    result["slug"] = slug_->text().trimmed();
    result["draft"] = draft_->isChecked();
    return result;
}
