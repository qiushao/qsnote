#include "MarkdownPreview.h"
#include <QDesktopServices>
#include <QCheckBox>
#include <QContextMenuEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPointer>
#include <QPushButton>
#include <QSettings>
#include <QShortcut>
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
    QWebEngineView::hideEvent(event);
}

void MarkdownPreview::setMarkdown(const QString &markdown, const QString &documentId) {
    if (loading_ && markdown == markdown_ && documentId == documentId_) return;
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
