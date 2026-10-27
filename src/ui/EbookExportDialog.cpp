#include "EbookExportDialog.h"
#include "UiStyle.h"
#include "core/EbookExporter.h"
#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QEventLoop>
#include <QFileDialog>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressDialog>
#include <QSettings>
#include <QStandardPaths>

namespace {
QString defaultExportDirectory() {
    auto documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (documents.isEmpty()) documents = QDir::homePath();
    return documents + "/qsnote_exports";
}
}// namespace

EbookExportDialog::EbookExportDialog(const Notebook &book, const QString &noteId, QWidget *parent) : QDialog(parent), book_(book) {
    setObjectName("ebookExportDialog");
    setWindowTitle(tr("导出电子书"));
    resize(560, 300);
    auto *layout = column(this, 20, 16);
    auto *form = new QFormLayout;
    form->setSpacing(12);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    scope_ = new QComboBox(this);
    scope_->setObjectName("exportScope");
    scope_->addItem((book_.type == "normal" ? tr("整本笔记本 · ") : tr("整本电子书 · ")) + book_.name, QString());
    for (const auto &note: book_.notes) {
        if (note.id == noteId && !note.folder && note.kind == Note::Kind::Markdown) scope_->addItem(tr("当前文档 · ") + note.title, note.id);
    }
    format_ = new QComboBox(this);
    format_->setObjectName("exportFormat");
    format_->addItem(tr("EPUB · 电子书"), int(EbookExporter::Format::Epub));
    format_->addItem(tr("PDF · 打印文档"), int(EbookExporter::Format::Pdf));
    format_->addItem(tr("CHM · 帮助文档"), int(EbookExporter::Format::Chm));
    format_->addItem(tr("HTML · 网页"), int(EbookExporter::Format::Html));
    auto *directoryRow = new QWidget(this);
    auto *directoryLayout = row(directoryRow, 0, 0);
    const auto defaultDirectory = defaultExportDirectory();
    QDir().mkpath(defaultDirectory);
    directory_ = new QLineEdit(QSettings().value("ebookExport/directory", defaultDirectory).toString(), directoryRow);
    directory_->setObjectName("exportDirectory");
    auto *browse = button(tr("浏览…"), "browseExportDirectory", directoryRow);
    directoryLayout->addWidget(directory_, 1);
    directoryLayout->addWidget(browse);
    connect(browse, &QPushButton::clicked, this, [this] {
        const auto selected = QFileDialog::getExistingDirectory(this, tr("选择保存目录"), directory_->text());
        if (!selected.isEmpty()) directory_->setText(selected);
    });
    form->addRow(tr("导出范围"), scope_);
    form->addRow(tr("导出格式"), format_);
    form->addRow(tr("保存目录"), directoryRow);
    layout->addLayout(form);
    layout->addWidget(label(tr("整本导出保留章节层级和顺序；当前文档仅导出该文档。已有同名文件时自动添加序号。"), this, true));
    auto *hint = label({}, this, true);
    hint->setObjectName("exportHint");
    layout->addWidget(hint);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    auto *confirm = buttons->button(QDialogButtonBox::Save);
    confirm->setObjectName("confirmExport");
    setPrimaryButton(confirm);
    confirm->setText(tr("导出"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("取消"));
    const auto validate = [this, hint, confirm] {
        const bool missingCompiler = format_->currentData().toInt() == int(EbookExporter::Format::Chm) && EbookExporter::chmCompiler().isEmpty();
        const bool validDirectory = QFileInfo(directory_->text().trimmed()).isDir();
        hint->setText(missingCompiler         ? tr("CHM 需要安装 Free Pascal 的 chmcmd（Linux：fp-utils）；Windows 也可将 HTML Help Workshop 的 hhc 加入 PATH。")
                      : !validDirectory       ? tr("请选择已有的保存目录。")
                      : book_.notes.isEmpty() ? (book_.type == "normal" ? tr("笔记本中还没有笔记。") : tr("电子书中还没有章节。"))
                                              : QString());
        confirm->setEnabled(!missingCompiler && validDirectory && !book_.notes.isEmpty());
    };
    connect(format_, &QComboBox::currentIndexChanged, this, validate);
    connect(directory_, &QLineEdit::textChanged, this, validate);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addStretch();
    layout->addWidget(buttons);
    validate();
}

QString EbookExportDialog::run() {
    if (exec() != QDialog::Accepted) return {};
    const auto destination = directory_->text().trimmed();
    QSettings().setValue("ebookExport/directory", destination);
    // Editor changes already update the notebook synchronously; export that snapshot, including unsaved edits.
    const auto snapshot = book_;
    QProgressDialog progress(tr("正在导出电子书…"), QString(), 0, 0, parentWidget());
    progress.setWindowTitle(tr("导出电子书"));
    progress.setWindowModality(Qt::ApplicationModal);
    progress.setCancelButton(nullptr);
    progress.setMinimumDuration(0);
    progress.setMinimumWidth(520);
    progress.setAutoClose(false);
    progress.setAutoReset(false);
    auto *status = new QLabel(tr("正在导出电子书…"), &progress);
    status->setTextFormat(Qt::PlainText);
    status->setWordWrap(true);
    progress.setLabel(status);
    progress.show();
    QString error;
    const auto path = EbookExporter::exportBook(snapshot, scope_->currentData().toString(),
                                                static_cast<EbookExporter::Format>(format_->currentData().toInt()), destination, &error,
                                                [&progress](int completed, int total, const QString &message) {
                                                    progress.setRange(0, total);
                                                    progress.setLabelText(message);
                                                    progress.setValue(completed);
                                                    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
                                                });
    progress.close();
    if (path.isEmpty()) QMessageBox::warning(parentWidget(), tr("导出失败"), error);
    return path;
}
