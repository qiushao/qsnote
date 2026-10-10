#include "MarkdownPreview.h"
#include <QDesktopServices>
#include <QCheckBox>
#include <QContextMenuEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFontDatabase>
#include <QFormLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPointer>
#include <QPushButton>
#include <QSettings>
#include <QShortcut>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWebChannel>
#include <QWebEnginePage>
#include <QWebEngineSettings>
#include <QWebEngineFindTextResult>

namespace {
class PreviewPage : public QWebEnginePage {
public:
    using QWebEnginePage::QWebEnginePage;

protected:
    bool acceptNavigationRequest(const QUrl &url, NavigationType type, bool mainFrame) override {
        if (type == NavigationTypeLinkClicked) {
            if (url.adjusted(QUrl::RemoveFragment) == QUrl("qrc:/web/preview.html")) return true;
            if (url.scheme() == "https" || url.scheme() == "http" || url.scheme() == "mailto") QDesktopServices::openUrl(url);
            return false;
        }
        return !mainFrame || url.adjusted(QUrl::RemoveFragment) == QUrl("qrc:/web/preview.html");
    }
};
}// namespace

MarkdownPreview::MarkdownPreview(QWidget *parent) : QWebEngineView(parent), bridge_(new PreviewBridge(this)) {
    setObjectName("preview");
    setPage(new PreviewPage(this));
    auto *channel = new QWebChannel(page());
    channel->registerObject("previewBridge", bridge_);
    page()->setWebChannel(channel);
    settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);
    settings()->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, false);
    connect(bridge_, &PreviewBridge::readyReceived, this, [this] {
        ready_ = true;
        applyReaderSettings();
        setHeadingFoldingEnabled(QSettings().value("reader/headingFolding", false).toBool());
        emit bridge_->documentChanged(markdown_, revision_, true);
        if (pendingLine_ >= 0) scrollToLine(pendingLine_);
    });
    connect(bridge_, &PreviewBridge::renderedReceived, this, [this](int revision) {
        if (revision != revision_) return;
        renderedRevision_ = revision;
        if (findDialog_ && findDialog_->isVisible()) findInPreview(false, true);
        emit renderFinished();
    });
    connect(bridge_, &PreviewBridge::sourceLineReceived, this, [this](int revision, int line) {
        if (revision == revision_ && line >= 0) emit sourceLineChanged(line);
    });
    connect(bridge_, &PreviewBridge::taskChangedReceived, this, [this](int revision, int line, bool checked) {
        if (revision == revision_ && line >= 0) emit taskToggled(line, checked);
    });
    auto *headings = new QShortcut(QKeySequence("Ctrl+G"), this);
    headings->setContext(Qt::WidgetWithChildrenShortcut);
    connect(headings, &QShortcut::activated, this, &MarkdownPreview::showHeadingNavigation);
}

void MarkdownPreview::applyReaderSettings() {
    if (!ready_) return;
    QSettings settings;
    settings.beginGroup("reader");
    const QJsonObject options{
        {"theme", settings.value("theme", "light").toString()},
        {"fontFamily", settings.value("fontFamily").toString()},
        {"fontSize", qBound(8, settings.value("fontSize", 16).toInt(), 72)},
        {"lineHeight", qBound(1.0, settings.value("lineHeight", 1.75).toDouble(), 3.0)},
        {"contentWidth", qBound(0, settings.value("contentWidth", 0).toInt(), 3000)}
    };
    page()->runJavaScript("window.qsnotePreview.setReaderStyle(" + QString::fromUtf8(QJsonDocument(options).toJson(QJsonDocument::Compact)) + ")");
}

void MarkdownPreview::showSettings() {
    QDialog dialog(this);
    dialog.setObjectName("readerSettings");
    dialog.setWindowTitle("阅读设置（所有笔记）");
    auto *layout = new QFormLayout(&dialog);
    QSettings settings;
    settings.beginGroup("reader");
    auto *theme = new QComboBox(&dialog);
    theme->setObjectName("readerTheme");
    theme->addItem("浅色", "light");
    theme->addItem("深色", "dark");
    theme->addItem("纸张", "sepia");
    theme->setCurrentIndex(qMax(0, theme->findData(settings.value("theme", "light"))));
    layout->addRow("配色", theme);
    auto *font = new QComboBox(&dialog);
    font->setObjectName("readerFont");
    font->addItem("系统默认", QString());
    for (const auto &family: QFontDatabase::families()) font->addItem(family, family);
    font->setCurrentIndex(qMax(0, font->findData(settings.value("fontFamily", QString()))));
    layout->addRow("字体", font);
    auto *size = new QSpinBox(&dialog);
    size->setObjectName("readerFontSize");
    size->setRange(8, 72);
    size->setValue(settings.value("fontSize", 16).toInt());
    layout->addRow("字号（px）", size);
    auto *spacing = new QDoubleSpinBox(&dialog);
    spacing->setObjectName("readerLineHeight");
    spacing->setRange(1.0, 3.0);
    spacing->setSingleStep(0.05);
    spacing->setValue(settings.value("lineHeight", 1.75).toDouble());
    layout->addRow("行距倍数", spacing);
    auto *width = new QSpinBox(&dialog);
    width->setObjectName("readerContentWidth");
    width->setRange(0, 3000);
    width->setValue(settings.value("contentWidth", 0).toInt());
    layout->addRow("最大正文宽度（px，0 为不限）", width);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return;
    settings.setValue("theme", theme->currentData());
    settings.setValue("fontFamily", font->currentData());
    settings.setValue("fontSize", size->value());
    settings.setValue("lineHeight", spacing->value());
    settings.setValue("contentWidth", width->value());
    settings.sync();
    applyReaderSettings();
}

void MarkdownPreview::showHeadingNavigation() {
    if (headingDialog_) {
        headingDialog_->raise();
        headingDialog_->activateWindow();
        return;
    }
    if (renderedRevision_ != revision_) return;
    auto *dialog = new QDialog(this);
    headingDialog_ = dialog;
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setObjectName("readerHeadings");
    dialog->setWindowTitle("跳转到标题");
    dialog->resize(420, 360);
    auto *layout = new QVBoxLayout(dialog);
    auto *query = new QLineEdit(dialog);
    query->setObjectName("readerHeadingFilter");
    query->setPlaceholderText("搜索标题");
    layout->addWidget(query);
    auto *list = new QListWidget(dialog);
    list->setObjectName("readerHeadingList");
    layout->addWidget(list);
    auto *hint = new QLabel("正在读取标题…", dialog);
    layout->addWidget(hint);
    auto filter = [list](const QString &text) {
        QListWidgetItem *first = nullptr;
        for (int i = 0; i < list->count(); ++i) {
            auto *item = list->item(i);
            item->setHidden(!item->text().contains(text, Qt::CaseInsensitive));
            if (!item->isHidden() && !first) first = item;
        }
        list->setCurrentItem(first);
    };
    connect(query, &QLineEdit::textChanged, dialog, filter);
    const int generation = revision_;
    const QPointer<QDialog> guard(dialog);
    page()->runJavaScript("window.qsnotePreview.headings()", [guard, list, hint, query, filter](const QVariant &value) {
        if (!guard || !guard->isVisible()) return;
        const auto headings = value.toList();
        for (int i = 0; i < headings.size(); ++i) {
            const auto heading = headings[i].toMap();
            auto *item = new QListWidgetItem(QString(qMax(0, heading.value("level").toInt() - 1) * 2, ' ') + heading.value("title").toString(), list);
            item->setData(Qt::UserRole, i);
        }
        hint->setText(headings.isEmpty() ? "当前笔记没有标题" : "输入关键词筛选，回车或双击跳转");
        filter(query->text());
    });
    auto jump = [this, dialog, list, generation] {
        const auto *item = list->currentItem();
        if (!item || item->isHidden() || revision_ != generation) return;
        page()->runJavaScript(QStringLiteral("window.qsnotePreview.jumpToHeading(%1, %2)").arg(generation).arg(item->data(Qt::UserRole).toInt()));
        dialog->close();
        setFocus();
    };
    connect(query, &QLineEdit::returnPressed, dialog, jump);
    connect(list, &QListWidget::itemActivated, dialog, jump);
    auto *close = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
    layout->addWidget(close);
    connect(close, &QDialogButtonBox::rejected, dialog, &QDialog::close);
    dialog->show();
    query->setFocus();
}

void MarkdownPreview::showFind() {
    if (!findDialog_) {
        findDialog_ = new QDialog(this);
        findDialog_->setObjectName("previewFind");
        findDialog_->setWindowTitle("阅读区查找");
        auto *layout = new QVBoxLayout(findDialog_);
        findQuery_ = new QLineEdit(findDialog_);
        findQuery_->setObjectName("previewFindText");
        findQuery_->setPlaceholderText("查找阅读内容（会展开折叠章节）");
        layout->addWidget(findQuery_);
        findCase_ = new QCheckBox("区分大小写", findDialog_);
        layout->addWidget(findCase_);
        findResult_ = new QLabel(findDialog_);
        findResult_->setObjectName("previewFindResult");
        layout->addWidget(findResult_);
        auto *buttons = new QHBoxLayout;
        layout->addLayout(buttons);
        for (const bool backward: {true, false}) {
            auto *button = new QPushButton(backward ? "上一个" : "下一个", findDialog_);
            button->setObjectName(backward ? "previewFindPrevious" : "previewFindNext");
            button->setAutoDefault(false);
            connect(button, &QPushButton::clicked, this, [this, backward] { findInPreview(backward); });
            buttons->addWidget(button);
        }
        auto *close = new QDialogButtonBox(QDialogButtonBox::Close, findDialog_);
        layout->addWidget(close);
        connect(close, &QDialogButtonBox::rejected, findDialog_, &QDialog::reject);
        connect(findQuery_, &QLineEdit::textChanged, this, [this] { findInPreview(false, true); });
        connect(findQuery_, &QLineEdit::returnPressed, this, [this] { findInPreview(); });
        connect(findCase_, &QCheckBox::toggled, this, [this] { findInPreview(false, true); });
        for (const bool backward: {true, false}) {
            auto *shortcut = new QShortcut(backward ? QKeySequence::FindPrevious : QKeySequence::FindNext, findDialog_);
            connect(shortcut, &QShortcut::activated, this, [this, backward] { findInPreview(backward); });
        }
        connect(findDialog_, &QDialog::finished, this, [this] {
            ++findRequest_;
            page()->findText({});
            if (isVisible()) setFocus();
        });
    }
    const auto selected = page()->selectedText();
    findDialog_->show();
    findDialog_->raise();
    findDialog_->activateWindow();
    if (!selected.isEmpty()) findQuery_->setText(selected);
    else findInPreview(false, true);
    findQuery_->selectAll();
    findQuery_->setFocus();
}

void MarkdownPreview::findInPreview(bool backward, bool restart) {
    const int request = ++findRequest_;
    const int generation = revision_;
    const auto query = findQuery_->text();
    if (renderedRevision_ != revision_) {
        findResult_->setText("正在渲染…");
        return;
    }
    QWebEnginePage::FindFlags flags;
    if (backward) flags |= QWebEnginePage::FindBackward;
    if (findCase_->isChecked()) flags |= QWebEnginePage::FindCaseSensitively;
    // A changed option or document must start a new Chromium find session.
    if (restart) page()->findText({});
    const QPointer<MarkdownPreview> guard(this);
    // Chromium searches visible text. Reveal sections before invoking it.
    page()->runJavaScript(query.isEmpty() ? QString() : QStringLiteral("window.qsnotePreview.expandAll()"),
                         [guard, request, generation, query, flags](const QVariant &) {
        if (!guard || request != guard->findRequest_ || generation != guard->revision_) return;
        guard->page()->findText(query, flags, [guard, request, generation, query](const QWebEngineFindTextResult &result) {
            if (!guard || request != guard->findRequest_ || generation != guard->revision_) return;
            guard->findResult_->setText(query.isEmpty() ? QString() : QStringLiteral("%1 / %2 个匹配").arg(result.activeMatch()).arg(result.numberOfMatches()));
        });
    });
}

void MarkdownPreview::setHeadingFoldingEnabled(bool enabled) {
    QSettings().setValue("reader/headingFolding", enabled);
    if (ready_) page()->runJavaScript(QStringLiteral("window.qsnotePreview.setHeadingFolding(%1)").arg(enabled ? "true" : "false"));
}

void MarkdownPreview::contextMenuEvent(QContextMenuEvent *event) {
    QScopedPointer<QMenu> menu(createStandardContextMenu());
    menu->addSeparator();
    menu->addAction("查找阅读内容…", this, &MarkdownPreview::showFind);
    auto *headings = menu->addAction("跳转到标题…", this, &MarkdownPreview::showHeadingNavigation);
    headings->setShortcut(QKeySequence("Ctrl+G"));
    headings->setEnabled(renderedRevision_ == revision_);
    menu->addAction("阅读设置…", this, &MarkdownPreview::showSettings);
    auto *fold = menu->addAction("标题折叠");
    fold->setCheckable(true);
    fold->setChecked(QSettings().value("reader/headingFolding", false).toBool());
    connect(fold, &QAction::triggered, this, &MarkdownPreview::setHeadingFoldingEnabled);
    if (fold->isChecked()) {
        menu->addAction("展开所有章节", this, [this] { page()->runJavaScript("window.qsnotePreview.expandAll()"); });
        menu->addAction("折叠所有章节", this, [this] { page()->runJavaScript("window.qsnotePreview.expandAll(false)"); });
    }
    menu->exec(event->globalPos());
}

void MarkdownPreview::hideEvent(QHideEvent *event) {
    if (findDialog_) findDialog_->close();
    if (headingDialog_) headingDialog_->close();
    QWebEngineView::hideEvent(event);
}

void MarkdownPreview::setMarkdown(const QString &markdown, const QString &documentId) {
    if (loading_ && markdown == markdown_ && documentId == documentId_) return;
    if (headingDialog_) headingDialog_->close();
    const bool reset = documentId != documentId_;
    if (reset) pendingLine_ = -1;
    markdown_ = markdown;
    documentId_ = documentId;
    ++revision_;
    ++findRequest_;
    if (!loading_) {
        loading_ = true;
        load(QUrl("qrc:/web/preview.html"));
    } else if (ready_) {
        emit bridge_->documentChanged(markdown_, revision_, reset);
    }
}

void MarkdownPreview::scrollToLine(int line) {
    if (ready_) emit bridge_->scrollRequested(revision_, qMax(0, line));
    else
        pendingLine_ = qMax(0, line);
}
