#include "MarkdownPreview.h"
#include "Theme.h"
#include "UiStyle.h"
#include "core/PlantUmlRenderer.h"
#include "core/WebTranslations.h"
#include <QDesktopServices>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFontDatabase>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QMimeDatabase>
#include <QFormLayout>
#include <QJsonDocument>
#include <QJsonArray>
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
#include <QTimer>
#include <QVBoxLayout>
#include <QWebChannel>
#include <QWebEngineDownloadRequest>
#include <QWebEnginePage>
#include <QWebEngineProfile>
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

QString PreviewBridge::localImage(const QString &url) { // NOLINT(readability-convert-member-functions-to-static): WebChannel slot.
    const QUrl location(url);
    if (!location.isLocalFile()) return {};
    const auto path = location.toLocalFile();
    if (!QImageReader(path).canRead()) return {};
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    const auto mime = QMimeDatabase().mimeTypeForFile(path, QMimeDatabase::MatchContent).name();
    return "data:" + mime + ";base64," + QString::fromLatin1(file.readAll().toBase64());
}

void PreviewBridge::renderPlantUml(const QString &source, int revision, int id) {
    auto *renderer = new PlantUmlRenderer(this);
    connect(renderer, &PlantUmlRenderer::finished, this, [this, renderer, revision, id](const QByteArray &png, const QString &error) {
        emit plantUmlRendered(revision, id, QString::fromLatin1(png.toBase64()), error);
        renderer->deleteLater();
    });
    renderer->render(source);
}

MarkdownPreview::MarkdownPreview(QWidget *parent) : QWebEngineView(parent), bridge_(new PreviewBridge(this)) {
    setObjectName("preview");
    setPage(new PreviewPage(this));
    installWebTranslations(page());
    auto *channel = new QWebChannel(page());
    channel->registerObject("previewBridge", bridge_);
    page()->setWebChannel(channel);
    settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);
    settings()->setAttribute(QWebEngineSettings::JavascriptCanOpenWindows, false);
    // The standard "Save image" action emits a download request that the application
    // must accept, otherwise Qt WebEngine silently cancels it and the item does
    // nothing. Local images are inlined as data URLs, so no other path saves them.
    connect(page()->profile(), &QWebEngineProfile::downloadRequested, this, [this](QWebEngineDownloadRequest *download) {
        if (download->page() != page()) return;
        const auto mime = download->mimeType();
        if (!mime.isEmpty() && !mime.startsWith("image/")) return;
        auto name = download->suggestedFileName();
        if (name.isEmpty()) name = QStringLiteral("image");
        const auto suffix = QMimeDatabase().mimeTypeForName(mime).preferredSuffix();
        if (!suffix.isEmpty() && QFileInfo(name).suffix().isEmpty()) name += '.' + suffix;
        const auto path = QFileDialog::getSaveFileName(this, tr("保存图片"), name, tr("图片 (*.png *.jpg *.jpeg *.gif *.webp *.bmp *.svg)"));
        if (path.isEmpty()) {
            download->cancel();
            return;
        }
        const QFileInfo target(path);
        download->setDownloadDirectory(target.absolutePath());
        download->setDownloadFileName(target.fileName());
        download->accept();
    });
    connect(bridge_, &PreviewBridge::readyReceived, this, [this] {
        ready_ = true;
        applyReaderSettings();
        setHeadingFoldingEnabled(QSettings().value("reader/headingFolding", false).toBool());
        emit bridge_->documentChanged(markdown_, revision_, true, baseUrl_);
        if (pendingLine_ >= 0) scrollToLine(pendingLine_, pendingLineCentered_);
    });
    connect(bridge_, &PreviewBridge::renderedReceived, this, [this](int revision) {
        if (revision != revision_) return;
        renderedRevision_ = revision;
        if (!pendingAnchor_.isEmpty()) scrollToAnchor(pendingAnchor_);
        if (findDialog_ && findDialog_->isVisible()) findInPreview(false, true);
    });
    connect(bridge_, &PreviewBridge::sourceLineReceived, this, [this](int revision, int line) {
        if (revision == revision_ && line >= 0) emit sourceLineChanged(line);
    });
    connect(bridge_, &PreviewBridge::taskChangedReceived, this, [this](int revision, int line, bool checked) {
        if (revision == revision_ && line >= 0) emit taskToggled(line, checked);
    });
    connect(bridge_, &PreviewBridge::localLinkRequested, this, [this](int revision, const QString &url) {
        const QUrl target(url);
        if (revision == revision_ && revision == renderedRevision_ && target.isLocalFile()) emit localLinkActivated(target);
    });
    connect(bridge_, &PreviewBridge::diagramPngReceived, this, [this](int requestId, const QString &data) {
        if (requestId != captureId_ || capture_ == Capture::None || data.isEmpty()) return;
        const auto mode = capture_;
        const auto path = capturePath_;
        capture_ = Capture::None;
        capturePath_.clear();
        const QByteArray png = QByteArray::fromBase64(data.toLatin1());
        if (mode == Capture::File) {
            QFile file(path);
            if (file.open(QIODevice::WriteOnly)) file.write(png);
            return;
        }
        QImage image;
        if (image.loadFromData(png, "PNG")) QApplication::clipboard()->setImage(image);
    });
    auto *headings = new QShortcut(QKeySequence("Ctrl+G"), this);
    headings->setContext(Qt::WidgetWithChildrenShortcut);
    connect(headings, &QShortcut::activated, this, &MarkdownPreview::showHeadingNavigation);
}

void MarkdownPreview::applyReaderSettings() {
    if (!ready_) return;
    QSettings settings;
    settings.beginGroup("reader");
    // 阅读区与窗口同色，主题模式和主题色改变时背景一起偏移。
    const QJsonObject options{
        {"theme", Theme::isDark() ? "dark" : "light"},
        {"surface", Theme::color("window").name()},
        {"foreground", Theme::color("text").name()},
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
    dialog.setWindowTitle(tr("阅读设置（所有笔记）"));
    auto *layout = new QFormLayout(&dialog);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(12);
    layout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    layout->setRowWrapPolicy(QFormLayout::WrapLongRows);
    QSettings settings;
    settings.beginGroup("reader");
    auto *font = new QComboBox(&dialog);
    font->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    font->setMinimumContentsLength(16);
    font->setObjectName("readerFont");
    font->addItem(tr("系统默认"), QString());
    for (const auto &family: QFontDatabase::families()) font->addItem(family, family);
    font->setCurrentIndex(qMax(0, font->findData(settings.value("fontFamily", QString()))));
    layout->addRow(tr("字体"), font);
    auto *size = new QSpinBox(&dialog);
    size->setObjectName("readerFontSize");
    size->setRange(8, 72);
    size->setValue(settings.value("fontSize", 16).toInt());
    layout->addRow(tr("字号（px）"), size);
    auto *spacing = new QDoubleSpinBox(&dialog);
    spacing->setObjectName("readerLineHeight");
    spacing->setRange(1.0, 3.0);
    spacing->setSingleStep(0.05);
    spacing->setValue(settings.value("lineHeight", 1.75).toDouble());
    layout->addRow(tr("行距倍数"), spacing);
    auto *width = new QSpinBox(&dialog);
    width->setObjectName("readerContentWidth");
    width->setRange(0, 3000);
    width->setValue(settings.value("contentWidth", 0).toInt());
    layout->addRow(tr("最大正文宽度（px，0 为不限）"), width);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    setPrimaryButton(buttons->button(QDialogButtonBox::Ok));
    layout->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return;
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
    dialog->setWindowTitle(tr("跳转到标题"));
    dialog->resize(420, 360);
    auto *layout = new QVBoxLayout(dialog);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(12);
    auto *query = new QLineEdit(dialog);
    query->setObjectName("readerHeadingFilter");
    query->setPlaceholderText(tr("搜索标题"));
    layout->addWidget(query);
    auto *list = new QListWidget(dialog);
    list->setObjectName("readerHeadingList");
    layout->addWidget(list);
    auto *hint = new QLabel(tr("正在读取标题…"), dialog);
    hint->setProperty("dim", true);
    hint->setWordWrap(true);
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
        hint->setText(headings.isEmpty() ? tr("当前笔记没有标题") : tr("输入关键词筛选，回车或双击跳转"));
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
        findDialog_->setWindowTitle(tr("阅读区查找"));
        auto *layout = new QVBoxLayout(findDialog_);
        layout->setContentsMargins(20, 20, 20, 20);
        layout->setSpacing(12);
        findQuery_ = new QLineEdit(findDialog_);
        findQuery_->setObjectName("previewFindText");
        findQuery_->setPlaceholderText(tr("查找阅读内容（会展开折叠章节）"));
        layout->addWidget(findQuery_);
        findCase_ = new QCheckBox(tr("区分大小写"), findDialog_);
        layout->addWidget(findCase_);
        findResult_ = new QLabel(findDialog_);
        findResult_->setObjectName("previewFindResult");
        findResult_->setProperty("dim", true);
        layout->addWidget(findResult_);
        auto *buttons = new QHBoxLayout;
        layout->addLayout(buttons);
        for (const bool backward: {true, false}) {
            auto *button = new QPushButton(backward ? tr("上一个") : tr("下一个"), findDialog_);
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
        findResult_->setText(tr("正在渲染…"));
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
            guard->findResult_->setText(query.isEmpty() ? QString() : tr("%1 / %2 个匹配").arg(result.activeMatch()).arg(result.numberOfMatches()));
        });
    });
}

void MarkdownPreview::setHeadingFoldingEnabled(bool enabled) {
    QSettings().setValue("reader/headingFolding", enabled);
    if (ready_) page()->runJavaScript(QStringLiteral("window.qsnotePreview.setHeadingFolding(%1)").arg(enabled ? "true" : "false"));
}

void MarkdownPreview::contextMenuEvent(QContextMenuEvent *event) {
    const QPoint globalPos = event->globalPos();
    const QPoint pos = event->pos();
    const QPointer<MarkdownPreview> guard(this);
    // Diagrams render as inline SVG, which the standard menu cannot copy. Hit-test the
    // page first; runJavaScript is asynchronous, so the menu is shown from its callback.
    page()->runJavaScript(QStringLiteral("window.qsnotePreview.diagramIdAt(%1,%2)").arg(pos.x()).arg(pos.y()),
                          [this, guard, globalPos](const QVariant &value) {
        if (!guard) return;
        showContextMenu(globalPos, value.toString());
    });
}

void MarkdownPreview::showContextMenu(const QPoint &globalPos, const QString &diagramId) {
    QScopedPointer<QMenu> menu(createStandardContextMenu());
    if (!diagramId.isEmpty()) {
        QAction *first = menu->actions().isEmpty() ? nullptr : menu->actions().constFirst();
        auto *copy = new QAction(tr("复制图片"), menu.data());
        // Defer until after menu->exec(): running the capture (and especially a modal
        // save dialog) from the action slot while the menu still holds the X11 grab
        // crashes Qt when the menu processes the next mouse event.
        connect(copy, &QAction::triggered, this, [this, diagramId] {
            QTimer::singleShot(0, this, [this, diagramId] { captureDiagram(diagramId, Capture::Clipboard); });
        });
        menu->insertAction(first, copy);
        auto *save = new QAction(tr("保存图片…"), menu.data());
        connect(save, &QAction::triggered, this, [this, diagramId] {
            QTimer::singleShot(0, this, [this, diagramId] { saveDiagram(diagramId); });
        });
        menu->insertAction(first, save);
        menu->insertSeparator(first);
    }
    menu->addSeparator();
    menu->addAction(tr("查找阅读内容…"), this, &MarkdownPreview::showFind);
    auto *headings = menu->addAction(tr("跳转到标题…"), this, &MarkdownPreview::showHeadingNavigation);
    headings->setShortcut(QKeySequence("Ctrl+G"));
    headings->setEnabled(renderedRevision_ == revision_);
    menu->addAction(tr("阅读设置…"), this, &MarkdownPreview::showSettings);
    auto *fold = menu->addAction(tr("标题折叠"));
    fold->setCheckable(true);
    fold->setChecked(QSettings().value("reader/headingFolding", false).toBool());
    connect(fold, &QAction::triggered, this, &MarkdownPreview::setHeadingFoldingEnabled);
    if (fold->isChecked()) {
        menu->addAction(tr("展开所有章节"), this, [this] { page()->runJavaScript("window.qsnotePreview.expandAll()"); });
        menu->addAction(tr("折叠所有章节"), this, [this] { page()->runJavaScript("window.qsnotePreview.expandAll(false)"); });
    }
    menu->exec(globalPos);
}

void MarkdownPreview::saveDiagram(const QString &diagramId) {
    const auto path = QFileDialog::getSaveFileName(this, tr("保存图片"), diagramId + ".png", tr("PNG 图片 (*.png)"));
    if (path.isEmpty()) return;
    captureDiagram(diagramId, Capture::File, path);
}

void MarkdownPreview::captureDiagram(const QString &diagramId, Capture mode, const QString &path) {
    capture_ = mode;
    capturePath_ = path;
    const int requestId = ++captureId_;
    page()->runJavaScript(QStringLiteral("window.qsnotePreview.renderDiagram(%1,\"%2\")").arg(requestId).arg(diagramId));
}

void MarkdownPreview::hideEvent(QHideEvent *event) {
    if (findDialog_) findDialog_->close();
    if (headingDialog_) headingDialog_->close();
    QWebEngineView::hideEvent(event);
}

void MarkdownPreview::setMarkdown(const QString &markdown, const QString &documentId, const QString &directory) {
    const auto baseUrl = directory.isEmpty() ? QString() : QUrl::fromLocalFile(directory + '/').toString(QUrl::FullyEncoded);
    if (loading_ && markdown == markdown_ && documentId == documentId_ && baseUrl == baseUrl_) return;
    if (headingDialog_) headingDialog_->close();
    const bool reset = documentId != documentId_;
    if (reset) {
        pendingLine_ = -1;
        pendingAnchor_.clear();
    }
    markdown_ = markdown;
    documentId_ = documentId;
    baseUrl_ = baseUrl;
    ++revision_;
    ++findRequest_;
    if (!loading_) {
        loading_ = true;
        load(QUrl("qrc:/web/preview.html"));
    } else if (ready_) {
        emit bridge_->documentChanged(markdown_, revision_, reset, baseUrl_);
    }
}

void MarkdownPreview::scrollToAnchor(const QString &anchor) {
    pendingAnchor_ = anchor;
    if (anchor.isEmpty() || renderedRevision_ != revision_) return;
    const auto arguments = QJsonDocument(QJsonArray{revision_, anchor}).toJson(QJsonDocument::Compact);
    page()->runJavaScript("window.qsnotePreview.jumpToAnchor(..." + QString::fromUtf8(arguments) + ")");
    pendingAnchor_.clear();
}

void MarkdownPreview::scrollToLine(int line, bool center) {
    if (ready_) emit bridge_->scrollRequested(revision_, qMax(0, line), center);
    else {
        pendingLine_ = qMax(0, line);
        pendingLineCentered_ = center;
    }
}
