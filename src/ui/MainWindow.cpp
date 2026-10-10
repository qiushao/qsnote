#include "MainWindow.h"
#include "FlowLayout.h"
#include "MarkdownEditor.h"
#include "MarkdownPreview.h"
#include <QScopedValueRollback>
#include <QScrollBar>
#include <QApplication>
#include <QButtonGroup>
#include <QCloseEvent>
#include <QComboBox>
#include <QDate>
#include <QDateEdit>
#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QDropEvent>
#include <QFileDialog>
#include <QFormLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <vtextedit/vtextedit.h>
#include <vtextedit/vmarkdowneditor.h>
#include <cmark.h>
#include <cstdlib>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScreen>
#include <QScrollArea>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStackedWidget>
#include <QStyledItemDelegate>
#include <QTabBar>
#include <QTextBlock>
#include <QTextBrowser>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <functional>

namespace {
QVBoxLayout *column(QWidget *widget, int margin = 0, int spacing = 0) {
    auto *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(margin, margin, margin, margin);
    layout->setSpacing(spacing);
    return layout;
}
QHBoxLayout *row(QWidget *widget, int horizontal = 10, int vertical = 8, int spacing = 6) {
    auto *layout = new QHBoxLayout(widget);
    layout->setContentsMargins(horizontal, vertical, horizontal, vertical);
    layout->setSpacing(spacing);
    return layout;
}
QLabel *label(const QString &text, QWidget *parent, bool dim = false) {
    auto *result = new QLabel(text, parent);
    result->setProperty("dim", dim);
    result->setWordWrap(true);
    return result;
}
QPushButton *button(const QString &text, const QString &name, QWidget *parent) {
    auto *result = new QPushButton(text, parent);
    result->setObjectName(name);
    result->setCursor(Qt::PointingHandCursor);
    return result;
}
QIcon icon(const QString &kind) {
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.scale(2, 2);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor("#8a8f98"), 1));
    if (kind == "left" || kind == "right") {
        painter.fillRect(QRectF(1, 1, 14, 14), QColor("#e3e5e8"));
        painter.drawRect(QRectF(1.5, 1.5, 13, 13));
        painter.drawLine(QPointF(kind == "left" ? 5 : 10, 2), QPointF(kind == "left" ? 5 : 10, 14));
    } else if (kind == "dirty") {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor("#3b6fd4"));
        painter.drawEllipse(QRectF(4.5, 4.5, 7, 7));
    } else if (kind == "folder") {
        painter.setBrush(QColor("#8a8f98"));
        painter.drawPolygon(QPolygonF{QPointF(1, 3), QPointF(6, 3), QPointF(8, 5), QPointF(15, 5), QPointF(15, 13), QPointF(1, 13)});
    } else {
        painter.drawRect(QRectF(3, 1, 10, 14));
        if (kind == "ebook") painter.drawLine(5, 2, 5, 14);
        for (int y: {5, 8, 11}) painter.drawLine(6, y, 10, y);
    }
    pixmap.setDevicePixelRatio(2);
    return {pixmap};
}

class NoteTabBar : public QTabBar {
public:
    using QTabBar::QTabBar;
    [[nodiscard]] QSize tabSizeHint(int index) const override {
        return {qBound(76, fontMetrics().horizontalAdvance(tabText(index)) + 64 + (tabIcon(index).isNull() ? 0 : 20), 200), 36};
    }
};

class NotebookDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        auto adjusted = option;
        initStyleOption(&adjusted, index);
        const auto type = index.data(Qt::UserRole + 1).toString();
        const int typeWidth = option.fontMetrics.horizontalAdvance(type) + 16;
        adjusted.text = option.fontMetrics.elidedText(adjusted.text, Qt::ElideRight, qMax(0, option.rect.width() - typeWidth - 20));
        option.widget->style()->drawControl(QStyle::CE_ItemViewItem, &adjusted, painter, option.widget);
        painter->save();
        painter->setPen(option.state & QStyle::State_Selected ? QColor("#3b6fd4") : QColor("#8a8f98"));
        auto font = option.font;
        font.setPixelSize(11);
        painter->setFont(font);
        painter->drawText(option.rect.adjusted(0, 0, -10, 0), Qt::AlignRight | Qt::AlignVCenter, type);
        painter->restore();
    }
    [[nodiscard]] QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        auto size = QStyledItemDelegate::sizeHint(option, index);
        size.setHeight(qMax(30, size.height()));
        return size;
    }
};

class NoteDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        const auto query = index.data(Qt::UserRole + 2).toString();
        if (query.isEmpty()) {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }
        auto adjusted = option;
        initStyleOption(&adjusted, index);
        adjusted.text.clear();
        option.widget->style()->drawControl(QStyle::CE_ItemViewItem, &adjusted, painter, option.widget);
        const auto rect = option.widget->style()->subElementRect(QStyle::SE_ItemViewItemText, &adjusted, option.widget).adjusted(3, 2, -6, 0);
        painter->save();
        painter->setClipRect(rect);
        auto drawLine = [&](const QString &text, int y, const QColor &color, int fontSize) {
            auto font = option.font;
            font.setPixelSize(fontSize);
            painter->setFont(font);
            const QFontMetrics metrics(font);
            const auto visible = metrics.elidedText(text, Qt::ElideRight, rect.width());
            int x = rect.left();
            qsizetype offset = 0;
            while (offset < visible.size()) {
                const auto hit = visible.indexOf(query, offset, Qt::CaseInsensitive);
                const auto prefix = visible.mid(offset, hit < 0 ? -1 : hit - offset);
                painter->setPen(color);
                painter->drawText(x, y + metrics.ascent(), prefix);
                x += metrics.horizontalAdvance(prefix);
                if (hit < 0) break;
                const auto match = visible.mid(hit, query.size());
                const int matchWidth = metrics.horizontalAdvance(match);
                painter->fillRect(x, y, matchWidth, metrics.height(), QColor("#ffe9a8"));
                painter->drawText(x, y + metrics.ascent(), match);
                x += matchWidth;
                offset = hit + query.size();
            }
        };
        drawLine(index.data().toString(), rect.top(), option.state & QStyle::State_Selected ? QColor("#3b6fd4") : QColor("#24292f"), 13);
        drawLine(index.data(Qt::UserRole + 3).toString(), rect.top() + 22, QColor("#8a8f98"), 12);
        painter->restore();
    }
    [[nodiscard]] QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        auto size = QStyledItemDelegate::sizeHint(option, index);
        if (!index.data(Qt::UserRole + 2).toString().isEmpty()) size.setHeight(48);
        return size;
    }
};

class SiteBrowser : public QTextBrowser {
public:
    using QTextBrowser::QTextBrowser;

protected:
    void resizeEvent(QResizeEvent *event) override {
        const int margin = qMax(0, (width() - 808) / 2);
        setViewportMargins(margin, 0, margin, 0);
        QTextBrowser::resizeEvent(event);
    }
};

class NoteTree : public QTreeWidget {
public:
    using QTreeWidget::QTreeWidget;
    std::function<void()> orderChanged;

protected:
    void dropEvent(QDropEvent *event) override {
        QTreeWidget::dropEvent(event);
        if (event->isAccepted() && orderChanged) orderChanged();
    }
};

// Parent-owned scrim keeps the dialogs visually inside the application.
class OverlayDialog : public QDialog {
public:
    OverlayDialog(QWidget *parent, const QString &name, bool wide = false) : QDialog(parent), scrim_(new QWidget(parent)) {
        setObjectName(name);
        setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
        setModal(true);
        scrim_->setStyleSheet("background: rgba(0,0,0,77);");
        scrim_->setGeometry(parent->rect());
        scrim_->show();
        scrim_->raise();
        const int preferred = wide ? 660 : 380;
        resize(qMin(preferred, parent->width() * 92 / 100), wide ? parent->height() * 80 / 100 : qMin(280, parent->height() * 80 / 100));
        setMaximumSize(parent->size() * 0.92);
    }
    ~OverlayDialog() override { delete scrim_; }

protected:
    void resizeEvent(QResizeEvent *event) override {
        QDialog::resizeEvent(event);
        QPainterPath shape;
        shape.addRoundedRect(QRectF(rect()), 10, 10);
        setMask(QRegion(shape.toFillPolygon().toPolygon()));
    }
    void showEvent(QShowEvent *event) override {
        QDialog::showEvent(event);
        move(parentWidget()->mapToGlobal(parentWidget()->rect().center()) - rect().center());
    }

private:
    QWidget *scrim_;
};

QString markdownHtml(const QString &markdown) {
    const auto utf8 = markdown.toUtf8();
    auto *html = cmark_markdown_to_html(utf8.constData(), static_cast<size_t>(utf8.size()), CMARK_OPT_DEFAULT);
    const auto result = QString::fromUtf8(html);
    std::free(html);
    return result;
}
const QString markdownStyle = QStringLiteral(
        "body { color: #24292f; font-size: 14px; line-height: 1.75; }"
        "h1 { font-size: 24px; } h2 { font-size: 20px; } h3 { font-size: 17px; }"
        "h1,h2,h3,h4,h5,h6 { margin-top: 20px; margin-bottom: 8px; }"
        "p { margin-top: 8px; margin-bottom: 8px; }"
        "pre { background-color: #f6f8fa; padding: 12px; }"
        "code { background-color: #f6f7f9; font-family: monospace; }"
        "blockquote { color: #8a8f98; background-color: #f6f7f9; margin-left: 14px; }"
        "th { background-color: #f6f7f9; } td,th { padding: 6px; }"
        "a { color: #3b6fd4; text-decoration: none; }");
}// namespace

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle(QCoreApplication::applicationName());
    QFile stylesheet(":/style.qss");
    if (stylesheet.open(QIODevice::ReadOnly)) setStyleSheet(QString::fromUtf8(stylesheet.readAll()));
    const auto available = screen()->availableGeometry().size();
    resize(available.width() * 4 / 5, available.height() * 4 / 5);
    buildUi();
    QString error;
    storageReady_ = store_.load(&error);
    if (!storageReady_) {
        QTimer::singleShot(0, this, [this, error] { QMessageBox::warning(this, "无法读取笔记", error); });
    }
    if (!store_.notebooks.isEmpty()) currentNotebookId_ = store_.notebooks.first().id;
    refreshNotebooks();
    refreshEditor();
    saveTimer_ = new QTimer(this);
    saveTimer_->setSingleShot(true);
    saveTimer_->setInterval(5000);
    connect(saveTimer_, &QTimer::timeout, this, [this] { save(); });
    auto *find = new QShortcut(QKeySequence::Find, this);
    connect(find, &QShortcut::activated, this, [this] {
        if (store_.note(currentNoteId())) {
            auto *focus = QApplication::focusWidget();
            if (viewMode_ == ViewMode::Preview || (viewMode_ == ViewMode::Split && focus && (focus == preview_ || preview_->isAncestorOf(focus)))) {
                preview_->showFind();
                return;
            }
            markdownEditor_->showFindReplace();
            return;
        }
        leftRequested_ = true;
        updatePanels();
        left_->show();
        search_->setFocus();
        search_->selectAll();
    });
    auto *findAll = new QShortcut(QKeySequence("Ctrl+Shift+F"), this);
    connect(findAll, &QShortcut::activated, this, [this] {
        leftRequested_ = true;
        updatePanels();
        left_->show();
        search_->setFocus();
        search_->selectAll();
    });
    auto *replace = new QShortcut(QKeySequence::Replace, this);
    connect(replace, &QShortcut::activated, this, [this] {
        if (!store_.note(currentNoteId())) return;
        if (viewMode_ == ViewMode::Preview) setPreview(false);
        markdownEditor_->showFindReplace();
    });
    auto *mode = new QShortcut(QKeySequence("Ctrl+E"), this);
    connect(mode, &QShortcut::activated, this, [this] { setPreview(!previewModeEnabled_); });
    auto *saveShortcut = new QShortcut(QKeySequence::Save, this);
    connect(saveShortcut, &QShortcut::activated, this, [this] { save(); });
    auto *escape = new QShortcut(QKeySequence(Qt::Key_Escape), search_);
    escape->setContext(Qt::WidgetShortcut);
    connect(escape, &QShortcut::activated, search_, &QLineEdit::clear);
}

MainWindow::~MainWindow() {
    for (const auto &session: editorSessions_) session.status->setParent(nullptr);
}

void MainWindow::buildUi() {
    auto *root = new QWidget(this);
    setCentralWidget(root);
    auto *layout = column(root);
    auto *header = new QWidget(root);
    header->setObjectName("header");
    header->setMinimumHeight(44);
    auto *headerLayout = row(header, 12, 8, 10);
    toggleLeft_ = button({}, "toggleLeft", header);
    toggleLeft_->setProperty("iconButton", true);
    toggleLeft_->setIcon(icon("left"));
    toggleLeft_->setToolTip("收起/展开笔记本栏");
    toggleLeft_->setAccessibleName(toggleLeft_->toolTip());
    headerLayout->addWidget(toggleLeft_);
    auto *title = label("MDNote", header);
    title->setObjectName("appTitle");
    headerLayout->addWidget(title);
    headerLayout->addStretch();
    siteConfig_ = button("站点配置", "siteConfig", header);
    sitePreview_ = button("站点预览", "sitePreview", header);
    export_ = button("导出", "exportNotebook", header);
    for (auto *action: {siteConfig_, sitePreview_, export_}) headerLayout->addWidget(action);
    toggleRight_ = button({}, "toggleRight", header);
    toggleRight_->setProperty("iconButton", true);
    toggleRight_->setIcon(icon("right"));
    toggleRight_->setToolTip("收起/展开大纲栏");
    toggleRight_->setAccessibleName(toggleRight_->toolTip());
    headerLayout->addWidget(toggleRight_);
    layout->addWidget(header);
    splitter_ = new QSplitter(Qt::Horizontal, root);
    splitter_->setObjectName("mainSplitter");
    splitter_->setHandleWidth(1);
    splitter_->setChildrenCollapsible(false);
    layout->addWidget(splitter_, 1);

    left_ = new QWidget(splitter_);
    left_->setObjectName("leftPanel");
    auto *leftLayout = column(left_);
    auto *leftHeader = new QWidget(left_);
    leftHeader->setObjectName("panelHeader");
    auto *leftHeaderLayout = row(leftHeader);
    auto *picker = new QWidget(leftHeader);
    picker->setObjectName("notebookPicker");
    auto *pickerLayout = row(picker, 0, 0, 0);
    notebooks_ = new QComboBox(picker);
    notebooks_->setObjectName("notebooks");
    notebooks_->setItemDelegate(new NotebookDelegate(notebooks_));
    notebooks_->setAccessibleName("当前笔记本");
    notebooks_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    notebooks_->setMinimumContentsLength(1);
    notebooks_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    pickerLayout->addWidget(notebooks_, 1);
    auto *add = button("＋", "newNotebook", picker);
    add->setProperty("iconButton", true);
    add->setToolTip("新建笔记本");
    add->setAccessibleName(add->toolTip());
    pickerLayout->addWidget(add);
    leftHeaderLayout->addWidget(picker);
    leftLayout->addWidget(leftHeader);
    auto *searchRow = new QWidget(left_);
    searchRow->setObjectName("searchRow");
    auto *searchLayout = row(searchRow);
    search_ = new QLineEdit(searchRow);
    search_->setObjectName("search");
    search_->setPlaceholderText("搜索当前笔记本 (Ctrl+Shift+F)");
    search_->setAccessibleName("搜索当前笔记本");
    search_->setMinimumWidth(0);
    searchLayout->addWidget(search_);
    leftLayout->addWidget(searchRow);
    auto *leftBody = new QWidget(left_);
    auto *bodyLayout = column(leftBody, 6, 4);
    groupBy_ = new QComboBox(leftBody);
    groupBy_->setObjectName("groupBy");
    groupBy_->addItems({"按分类", "按标签", "按日期"});
    bodyLayout->addWidget(groupBy_);
    treeHint_ = label({}, leftBody, true);
    treeHint_->setContentsMargins(10, 14, 10, 0);
    bodyLayout->addWidget(treeHint_);
    auto *noteTree = new NoteTree(leftBody);
    tree_ = noteTree;
    noteTree->orderChanged = [this] {
        auto *book = store_.notebook(currentNotebookId_);
        if (!book) return;
        QList<Note> reordered;
        QTreeWidgetItemIterator it(tree_);
        while (*it) {
            auto *entry = *it;
            auto *note = store_.note(entry->data(0, Qt::UserRole).toString());
            if (note) {
                note->parentId = entry->parent() ? entry->parent()->data(0, Qt::UserRole).toString() : QString();
                reordered.append(*note);
            }
            ++it;
        }
        book->notes = reordered;
        save();
    };
    tree_->setObjectName("noteTree");
    tree_->setItemDelegate(new NoteDelegate(tree_));
    tree_->setHeaderHidden(true);
    tree_->setIndentation(14);
    tree_->setIconSize(QSize(14, 14));
    tree_->setTextElideMode(Qt::ElideRight);
    tree_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    tree_->setContextMenuPolicy(Qt::CustomContextMenu);
    tree_->setMinimumSize(0, 0);
    bodyLayout->addWidget(tree_, 1);
    leftLayout->addWidget(leftBody, 1);

    auto *center = new QWidget(splitter_);
    center->setObjectName("center");
    center->setMinimumWidth(0);
    auto *centerLayout = column(center);
    auto *strip = new QWidget(center);
    strip->setObjectName("tabStrip");
    auto *stripLayout = row(strip, 0, 0, 0);
    tabs_ = new NoteTabBar(strip);
    tabs_->setObjectName("noteTabs");
    tabs_->setTabsClosable(true);
    tabs_->setExpanding(false);
    tabs_->setElideMode(Qt::ElideRight);
    tabs_->setUsesScrollButtons(true);
    tabs_->setDrawBase(false);
    strip->setMinimumHeight(36);
    stripLayout->addWidget(tabs_);
    centerLayout->addWidget(strip);
    toolbar_ = new QWidget(center);
    toolbar_->setObjectName("toolbar");
    buildToolbar();
    centerLayout->addWidget(toolbar_);
    metadata_ = new QWidget(center);
    metadata_->setObjectName("metadata");
    new FlowLayout(metadata_, 6, 10);
    centerLayout->addWidget(metadata_);
    pages_ = new QStackedWidget(center);
    pages_->setObjectName("editorPages");
    pages_->setMinimumSize(0, 0);
    auto *empty = new QWidget(pages_);
    empty->setObjectName("emptyState");
    auto *emptyLayout = column(empty, 12, 6);
    emptyLayout->addStretch();
    for (const auto &text: {"从左侧选择一篇笔记开始", "支持普通笔记本 / 静态站点 / 电子书 三种类型"}) {
        auto *hint = label(text, empty, true);
        hint->setAlignment(Qt::AlignCenter);
        emptyLayout->addWidget(hint);
    }
    emptyLayout->addStretch();
    pages_->addWidget(empty);
    editorSplitter_ = new QSplitter(Qt::Horizontal, pages_);
    editorSplitter_->setObjectName("editorSplitter");
    editorSplitter_->setChildrenCollapsible(false);
    pages_->addWidget(editorSplitter_);
    editors_ = new QStackedWidget(editorSplitter_);
    editors_->setObjectName("noteEditors");
    editors_->setMinimumSize(0, 0);
    editorSplitter_->addWidget(editors_);
    preview_ = new MarkdownPreview(editorSplitter_);
    editorSplitter_->addWidget(preview_);
    preview_->hide();
    previewTimer_ = new QTimer(this);
    previewTimer_->setSingleShot(true);
    previewTimer_->setInterval(180);
    connect(previewTimer_, &QTimer::timeout, this, [this] {
        if (const auto *note = store_.note(currentNoteId())) preview_->setMarkdown(note->content, note->id);
    });
    connect(preview_, &MarkdownPreview::sourceLineChanged, this, [this](int line) {
        if (!editor_ || !preview_->matchesDocument(editor_->toPlainText(), currentNoteId())) return;
        editorSessions_[activeEditorId_].previewLine = line;
        if (viewMode_ != ViewMode::Split) return;
        const QScopedValueRollback<bool> guard(syncingPreviewScroll_, true);
        markdownEditor_->scrollToLine(line, false);
    });
    connect(preview_, &MarkdownPreview::taskToggled, this, [this](int line, bool checked) {
        if (!editor_ || !preview_->matchesDocument(editor_->toPlainText(), currentNoteId())) return;
        const auto block = editor_->document()->findBlockByNumber(line);
        const QRegularExpression task(R"(^(?:\s*>)*\s*(?:[-*+]|\d+[.)])\s+\[([ xX])\])");
        const auto match = task.match(block.text());
        if (!match.hasMatch()) return;
        QTextCursor cursor(block);
        cursor.setPosition(block.position() + static_cast<int>(match.capturedStart(1)));
        cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
        cursor.insertText(checked ? "x" : " ");
    });
    centerLayout->addWidget(pages_, 1);
    status_ = label({}, center, true);
    status_->setObjectName("status");
    centerLayout->addWidget(status_);
    editorStatuses_ = new QStackedWidget(center);
    editorStatuses_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Maximum);
    centerLayout->addWidget(editorStatuses_);

    right_ = new QWidget(splitter_);
    right_->setObjectName("rightPanel");
    auto *rightLayout = column(right_);
    auto *rightHeader = new QWidget(right_);
    rightHeader->setObjectName("panelHeader");
    auto *rightHeaderLayout = row(rightHeader);
    auto *outlineTitle = label("大纲", rightHeader, true);
    outlineTitle->setObjectName("sectionTitle");
    rightHeaderLayout->addWidget(outlineTitle);
    rightLayout->addWidget(rightHeader);
    outlineHint_ = label("打开一篇笔记后自动生成大纲", right_, true);
    outlineHint_->setContentsMargins(16, 20, 16, 0);
    rightLayout->addWidget(outlineHint_);
    outline_ = new QTreeWidget(right_);
    outline_->setObjectName("outline");
    outline_->setHeaderHidden(true);
    outline_->setRootIsDecorated(false);
    outline_->setIndentation(14);
    outline_->setContentsMargins(6, 6, 6, 20);
    outline_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    outline_->setMinimumSize(0, 0);
    rightLayout->addWidget(outline_, 1);
    splitter_->setStretchFactor(0, 0);
    splitter_->setStretchFactor(1, 1);
    splitter_->setStretchFactor(2, 0);
    QTimer::singleShot(0, this, [this] {
        splitter_->setSizes({264, qMax(240, width() - 496), 232});
    });
    connect(add, &QPushButton::clicked, this, &MainWindow::createNotebook);
    connect(notebooks_, &QComboBox::currentIndexChanged, this, [this] {
        currentNotebookId_ = notebooks_->currentData().toString();
        search_->clear();
        refreshNotebooks();
    });
    connect(search_, &QLineEdit::textChanged, this, &MainWindow::refreshTree);
    connect(groupBy_, &QComboBox::currentIndexChanged, this, &MainWindow::refreshTree);
    connect(tree_, &QTreeWidget::customContextMenuRequested, this, &MainWindow::showTreeMenu);
    connect(tree_, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem *item) {
        const auto id = item->data(0, Qt::UserRole).toString();
        auto *note = store_.note(id);
        if (!note || note->folder) item->setExpanded(!item->isExpanded());
    });
    connect(tree_, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *item) {
        const auto id = item->data(0, Qt::UserRole).toString();
        auto *note = store_.note(id);
        if (note && !note->folder) openNote(id);
    });
    connect(tree_, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem *item) {
        const auto id = item->data(0, Qt::UserRole).toString();
        auto *note = store_.note(id);
        if (note && !note->folder) openNote(id);
    });
    connect(tabs_, &QTabBar::currentChanged, this, &MainWindow::refreshEditor);
    connect(tabs_, &QTabBar::tabCloseRequested, this, &MainWindow::closeTab);
    connect(toggleLeft_, &QPushButton::clicked, this, [this] {
        leftRequested_ = !left_->isVisible();
        left_->setVisible(leftRequested_);
        if (leftRequested_ && width() < 720) right_->hide();
    });
    connect(toggleRight_, &QPushButton::clicked, this, [this] {
        rightRequested_ = !right_->isVisible();
        right_->setVisible(rightRequested_);
        if (rightRequested_ && width() < 720) left_->hide();
    });
    connect(siteConfig_, &QPushButton::clicked, this, &MainWindow::showSiteConfig);
    connect(sitePreview_, &QPushButton::clicked, this, &MainWindow::showSitePreview);
    connect(export_, &QPushButton::clicked, this, &MainWindow::exportNotebook);
    connect(outline_, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem *item) {
        const int line = item->data(0, Qt::UserRole).toInt();
        if (previewModeEnabled_) {
            preview_->scrollToLine(line);
        } else {
            editor_->setTextCursor(QTextCursor(editor_->document()->findBlockByNumber(line)));
            editor_->ensureCursorVisible();
            if (viewMode_ == ViewMode::Split) preview_->scrollToLine(line);
            editor_->setFocus();
        }
    });
}

void MainWindow::buildToolbar() {
    auto *layout = new FlowLayout(toolbar_, 5, 2);
    using Type = vte::TypeAction;
    struct Tool {
        QString title;
        Type action = Type::TypeBold;
        QVariant data;
    };
    const QList<Tool> tools = {
        {"H1", Type::TypeHeading, 1}, {"H2", Type::TypeHeading, 2}, {"H3", Type::TypeHeading, 3}, {},
        {"B", Type::TypeBold}, {"高亮", Type::TypeMark}, {},
        {"链接", Type::TypeLink}, {"图片", Type::TypeImage}, {},
        {"列表", Type::TypeUnorderedList}, {"有序", Type::TypeOrderedList},
        {"任务", Type::TypeTodoList, false}, {"引用", Type::TypeQuote}, {},
        {"代码", Type::TypeCode}, {"代码块", Type::TypeCodeBlock},
        {"公式", Type::TypeMath}, {"公式块", Type::TypeMathBlock}, {"表格", Type::TypeTable}};
    for (const auto &tool: tools) {
        if (tool.title.isEmpty()) {
            auto *separator = new QFrame(toolbar_);
            separator->setFrameShape(QFrame::VLine);
            separator->setStyleSheet("color: #e3e5e8; margin: 4px 5px;");
            separator->setFixedSize(11, 24);
            layout->addWidget(separator);
            continue;
        }
        auto *action = button(tool.title, "tool" + tool.title, toolbar_);
        action->setProperty("tool", true);
        action->setToolTip(tool.title);
        action->setFocusPolicy(Qt::NoFocus);
        auto font = action->font();
        font.setBold(tool.title == "B");
        action->setFont(font);
        layout->addWidget(action);
        connect(action, &QPushButton::clicked, this, [this, tool] {
            if (!store_.note(currentNoteId())) return;
            setPreview(false);
            markdownEditor_->type(tool.action, tool.data);
        });
    }
    auto *more = button("更多", "editorOptions", toolbar_);
    layout->addWidget(more);
    auto *modes = new QWidget(toolbar_);
    modes->setProperty("alignRight", true);
    auto *modeLayout = row(modes, 0, 0, 0);
    editMode_ = button("编辑", "editMode", modes);
    previewMode_ = button("预览", "previewMode", modes);
    splitMode_ = button("分屏", "splitMode", modes);
    auto *group = new QButtonGroup(modes);
    for (auto *mode: {editMode_, previewMode_, splitMode_}) {
        mode->setProperty("mode", true);
        mode->setCheckable(true);
        group->addButton(mode);
        modeLayout->addWidget(mode);
    }
    layout->addWidget(modes);
    connect(editMode_, &QPushButton::clicked, this, [this] { setPreview(false); });
    connect(previewMode_, &QPushButton::clicked, this, [this] { setPreview(true); });
    connect(splitMode_, &QPushButton::clicked, this, [this] { setViewMode(ViewMode::Split); });
}

void MainWindow::refreshNotebooks() {
    const QSignalBlocker blocker(notebooks_);
    notebooks_->clear();
    for (const auto &book: store_.notebooks) {
        notebooks_->addItem(book.name, book.id);
        notebooks_->setItemData(notebooks_->count() - 1, NotebookStore::typeLabel(book.type), Qt::UserRole + 1);
        notebooks_->setItemData(notebooks_->count() - 1, book.name + " · " + NotebookStore::typeLabel(book.type), Qt::ToolTipRole);
    }
    notebooks_->setPlaceholderText("无笔记本");
    notebooks_->setCurrentIndex(notebooks_->findData(currentNotebookId_));
    auto *book = store_.notebook(currentNotebookId_);
    const bool site = book && book->type == "site";
    siteConfig_->setVisible(site);
    sitePreview_->setVisible(site);
    export_->setVisible(book && book->type != "normal");
    export_->setText(site ? "导出站点" : "导出电子书");
    updatePanels();
    refreshTree();
}

void MainWindow::refreshTree() {
    QSet<QString> collapsed;
    QTreeWidgetItemIterator iterator(tree_);
    while (*iterator) {
        if (!(*iterator)->isExpanded() && (*iterator)->childCount()) collapsed.insert((*iterator)->data(0, Qt::UserRole).toString());
        ++iterator;
    }
    tree_->clear();
    auto *book = store_.notebook(currentNotebookId_);
    groupBy_->setVisible(book && book->type == "site" && search_->text().isEmpty());
    treeHint_->hide();
    if (!book) return;
    const auto query = search_->text().trimmed();
    tree_->setDragDropMode(book->type == "normal" && query.isEmpty() ? QAbstractItemView::InternalMove : QAbstractItemView::NoDragDrop);
    QHash<QString, QTreeWidgetItem *> nodes;
    int matches = 0;
    for (const auto &note: book->notes) {
        if (!query.isEmpty() && (note.folder || (!note.title.contains(query, Qt::CaseInsensitive) && !note.content.contains(query, Qt::CaseInsensitive)))) continue;
        QStringList groups{QString()};
        if (book->type == "site" && query.isEmpty()) {
            if (groupBy_->currentIndex() == 0) groups = {note.metadata["category"].toString().isEmpty() ? QStringLiteral("未分类") : note.metadata["category"].toString()};
            else if (groupBy_->currentIndex() == 1) {
                groups = note.metadata["tags"].toString().split(QRegularExpression("[,，]"), Qt::SkipEmptyParts);
                for (auto &tag: groups) tag = tag.trimmed();
                groups.removeDuplicates();
                if (groups.isEmpty()) groups = {QStringLiteral("无标签")};
            } else
                groups = {note.metadata["date"].toString().left(7)};
        }
        for (const auto &group: groups) {
            auto *item = new QTreeWidgetItem;
            item->setText(0, note.title);
            if (!note.folder) item->setFlags(item->flags() & ~Qt::ItemIsDropEnabled);
            item->setIcon(0, icon(note.folder ? "folder" : book->type));
            item->setData(0, Qt::UserRole, note.id);
            item->setToolTip(0, note.title);
            if (!query.isEmpty()) {
                const auto hit = note.content.indexOf(query, 0, Qt::CaseInsensitive);
                const auto snippet = note.content.mid(qMax(0, hit - 20), 90).replace('\n', ' ');
                item->setData(0, Qt::UserRole + 2, query);
                item->setData(0, Qt::UserRole + 3, snippet);
                item->setToolTip(0, note.title + "\n" + snippet);
            }
            if (book->type == "site" && query.isEmpty()) {
                if (!nodes.contains(group)) {
                    auto *groupItem = new QTreeWidgetItem(tree_);
                    groupItem->setIcon(0, icon("folder"));
                    groupItem->setData(0, Qt::UserRole, "group:" + group);
                    nodes.insert(group, groupItem);
                }
                nodes[group]->addChild(item);
                nodes[group]->setText(0, group + QStringLiteral("（%1）").arg(nodes[group]->childCount()));
                nodes[group]->setExpanded(!collapsed.contains("group:" + group));
            } else {
                tree_->addTopLevelItem(item);
                nodes.insert(note.id, item);
            }
            if (note.id == currentNoteId()) tree_->setCurrentItem(item);
        }
        ++matches;
    }
    if (book->type != "site" && query.isEmpty()) {
        for (const auto &note: book->notes) {
            auto *item = nodes.value(note.id);
            auto *parent = nodes.value(note.parentId);
            if (item && parent) {
                tree_->takeTopLevelItem(tree_->indexOfTopLevelItem(item));
                parent->addChild(item);
            }
        }
        for (auto *item: nodes) item->setExpanded(!collapsed.contains(item->data(0, Qt::UserRole).toString()));
    }
    QTreeWidgetItemIterator active(tree_);
    while (*active) {
        if ((*active)->data(0, Qt::UserRole).toString() == currentNoteId()) tree_->setCurrentItem(*active);
        ++active;
    }
    if (!query.isEmpty()) treeHint_->setText(QStringLiteral("找到 %1 篇笔记").arg(matches));
    else
        treeHint_->setText(book->type == "site" ? "暂无文章，右键新建" : book->type == "ebook" ? "暂无章节，右键添加"
                                                                                               : "暂无内容，右键新建");
    treeHint_->setVisible(!query.isEmpty() || book->notes.isEmpty());
}

QString MainWindow::currentNoteId() const { return tabs_->tabData(tabs_->currentIndex()).toString(); }

void MainWindow::openNote(const QString &id) {
    for (int index = 0; index < tabs_->count(); ++index) {
        if (tabs_->tabData(index).toString() == id) {
            tabs_->setCurrentIndex(index);
            return;
        }
    }
    const auto *note = store_.note(id);
    if (!note || note->folder) return;
    {
        const QSignalBlocker blocker(tabs_);
        const int index = tabs_->addTab(note->title);
        tabs_->setTabData(index, id);
        tabs_->setTabToolTip(index, note->title);
        tabs_->setCurrentIndex(index);
    }
    previewModeEnabled_ = false;
    refreshEditor();
}

void MainWindow::closeTab(int index) {
    if (!save()) return;
    const auto id = tabs_->tabData(index).toString();
    {
        const QSignalBlocker blocker(tabs_);
        tabs_->removeTab(index);
    }
    removeEditor(id);
    refreshEditor();
}

void MainWindow::activateEditor(const QString &id) {
    if (activeEditorId_ == id) return;
    if (editorSessions_.contains(activeEditorId_)) {
        auto &previous = editorSessions_[activeEditorId_];
        previous.verticalScroll = editor_->verticalScrollBar()->value();
        previous.horizontalScroll = editor_->horizontalScrollBar()->value();
        markdownEditor_->setObjectName("inactiveMarkdownEditor");
        editor_->setObjectName("inactiveEditor");
    }
    previewTimer_->stop();
    if (!editorSessions_.contains(id)) {
        EditorSession session;
        auto *documentEditor = new MarkdownEditor(editors_);
        auto *textEdit = documentEditor->getTextEdit();
        textEdit->setPlaceholderText("开始书写 Markdown...");
        textEdit->setProperty("noteId", id);
        session.editor = documentEditor;
        session.status = documentEditor->statusWidget();
        session.status->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        session.menu = documentEditor->createEditorMenu(documentEditor);
        connect(session.menu, &QMenu::aboutToShow, this, [this] { setPreview(false); });
        editors_->addWidget(documentEditor);
        editorStatuses_->addWidget(session.status.data());
        editorSessions_.insert(id, session);
        connect(documentEditor, &MarkdownEditor::settingsChanged, this, [this, documentEditor] {
            for (const auto &session: editorSessions_) {
                if (session.editor != documentEditor) session.editor->reloadSettings();
            }
        });
        connect(documentEditor, &vte::VMarkdownEditor::headingsUpdated, this,
                [this, id, textEdit](const QVector<vte::md::HeadingInfo> &headings, bool) {
            auto &cached = editorSessions_[id].headings;
            cached.clear();
            for (const auto &heading: headings) {
                cached.append({heading.m_title, heading.m_level, textEdit->document()->findBlock(heading.m_startPos).blockNumber()});
            }
            if (activeEditorId_ == id) refreshOutline();
        });
        documentEditor->setText(store_.note(id)->content);
        connect(textEdit, &QTextEdit::textChanged, this, [this, id, textEdit] {
            auto *note = store_.note(id);
            if (!note) return;
            const auto text = textEdit->toPlainText();
            if (note->content == text) return;
            note->content = text;
            note->contentDirty = true;
            for (int index = 0; index < tabs_->count(); ++index) {
                if (tabs_->tabData(index).toString() == id) tabs_->setTabIcon(index, icon("dirty"));
            }
            if (activeEditorId_ == id) {
                updateStatus();
                if (viewMode_ != ViewMode::Edit) previewTimer_->start();
            }
            saveTimer_->start();
        });
        connect(textEdit->verticalScrollBar(), &QScrollBar::valueChanged, this, [this, id, documentEditor] {
            if (activeEditorId_ == id && viewMode_ == ViewMode::Split && !syncingPreviewScroll_) {
                preview_->scrollToLine(documentEditor->getTopLine());
            }
        });
    }
    auto &session = editorSessions_[id];
    activeEditorId_ = id;
    markdownEditor_ = session.editor;
    editor_ = markdownEditor_->getTextEdit();
    markdownEditor_->setObjectName("markdownEditor");
    editor_->setObjectName("editor");
    editors_->setCurrentWidget(markdownEditor_);
    editorStatuses_->setCurrentWidget(session.status.data());
    findChild<QPushButton *>("editorOptions")->setMenu(session.menu);
    setViewMode(session.mode);
    // Layout changes on show can scroll QTextEdit to its caret. Restore the
    // viewport after that layout without moving the retained cursor/selection.
    QTimer::singleShot(0, markdownEditor_, [this, id, vertical = session.verticalScroll, horizontal = session.horizontalScroll] {
        if (activeEditorId_ != id) return;
        const QScopedValueRollback<bool> guard(syncingPreviewScroll_, true);
        editor_->verticalScrollBar()->setValue(vertical);
        editor_->horizontalScrollBar()->setValue(horizontal);
        if (viewMode_ != ViewMode::Edit) {
            preview_->scrollToLine(viewMode_ == ViewMode::Split ? markdownEditor_->getTopLine() : editorSessions_[id].previewLine);
        }
    });
}

void MainWindow::removeEditor(const QString &id) {
    if (!editorSessions_.contains(id)) return;
    auto session = editorSessions_.take(id);
    if (activeEditorId_ == id) {
        activeEditorId_.clear();
        markdownEditor_ = nullptr;
        editor_ = nullptr;
        findChild<QPushButton *>("editorOptions")->setMenu(nullptr);
    }
    editorStatuses_->removeWidget(session.status.data());
    session.status->setParent(nullptr);
    session.status.clear();
    editors_->removeWidget(session.editor);
    disconnect(session.editor, nullptr, this, nullptr);
    disconnect(session.editor->getTextEdit(), nullptr, this, nullptr);
    disconnect(session.editor->getTextEdit()->verticalScrollBar(), nullptr, this, nullptr);
    delete session.editor;
}

void MainWindow::refreshEditor() {
    const auto id = currentNoteId();
    const auto *note = store_.note(id);
    const auto *book = store_.owner(id);
    toolbar_->setVisible(note);
    metadata_->setVisible(book && book->type == "site");
    while (auto *item = metadata_->layout()->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    if (note) {
        activateEditor(id);
        if (book->type == "site") {
            const QList<QPair<QString, QString>> fields = {{"category", "分类"}, {"tags", "标签"}, {"date", "日期"}, {"slug", "Slug"}};
            for (const auto &field: fields) {
                auto *container = new QWidget(metadata_);
                auto *layout = row(container, 0, 0, 5);
                auto *caption = label(field.second, container, true);
                if (field.first == "date") {
                    auto *date = new QDateEdit(QDate::fromString(note->metadata["date"].toString(), Qt::ISODate), container);
                    date->setObjectName("meta_date");
                    date->setAccessibleName("日期");
                    date->setDisplayFormat("yyyy-MM-dd");
                    date->setCalendarPopup(true);
                    layout->addWidget(caption);
                    layout->addWidget(date);
                    metadata_->layout()->addWidget(container);
                    connect(date, &QDateEdit::dateChanged, this, [this, id](const QDate &value) {
                        if (auto *target = store_.note(id)) target->metadata["date"] = value.toString(Qt::ISODate);
                        saveTimer_->start();
                        refreshTree();
                    });
                    continue;
                }
                auto *input = new QLineEdit(note->metadata[field.first].toString(), container);
                input->setObjectName("meta_" + field.first);
                input->setAccessibleName(field.second);
                input->setPlaceholderText(field.first == "category" ? "未分类" : field.first == "tags" ? "逗号分隔"
                                                                         : field.first == "date"       ? "YYYY-MM-DD"
                                                                                                       : "");
                input->setMaximumWidth(input->fontMetrics().horizontalAdvance(QString(field.first == "tags" ? 18 : 12, 'x')) + 20);
                layout->addWidget(caption);
                layout->addWidget(input);
                metadata_->layout()->addWidget(container);
                connect(input, &QLineEdit::textEdited, this, [this, id, key = field.first](const QString &value) {
                    if (auto *target = store_.note(id)) target->metadata[key] = value;
                    saveTimer_->start();
                    refreshTree();
                });
            }
        }
        setViewMode(viewMode_);
    } else {
        pages_->setCurrentIndex(0);
        previewTimer_->stop();
        if (preview_->revision() > 0) preview_->setMarkdown({}, {});
        editorStatuses_->hide();
        status_->clear();
    }
    refreshOutline();
    refreshTree();
}

void MainWindow::setPreview(bool enabled) {
    setViewMode(enabled ? ViewMode::Preview : ViewMode::Edit);
}

void MainWindow::setViewMode(ViewMode mode) {
    const auto *note = store_.note(currentNoteId());
    if (!note) return;
    const auto oldMode = viewMode_;
    viewMode_ = mode;
    previewModeEnabled_ = mode == ViewMode::Preview;
    editMode_->setChecked(mode == ViewMode::Edit);
    previewMode_->setChecked(mode == ViewMode::Preview);
    splitMode_->setChecked(mode == ViewMode::Split);
    editors_->setVisible(mode != ViewMode::Preview);
    editorSessions_[activeEditorId_].mode = mode;
    preview_->setVisible(mode != ViewMode::Edit);
    pages_->setCurrentIndex(1);
    if (mode != ViewMode::Edit) preview_->setMarkdown(note->content, note->id);
    else previewTimer_->stop();
    if (mode == ViewMode::Split && oldMode != ViewMode::Split) {
        editorSplitter_->setSizes({editorSplitter_->width() / 2, editorSplitter_->width() / 2});
        preview_->scrollToLine(markdownEditor_->getTopLine());
    }
    editorStatuses_->setVisible(mode != ViewMode::Preview);
    updateStatus();
}

void MainWindow::refreshOutline() {
    outline_->clear();
    const auto *note = store_.note(currentNoteId());
    outlineHint_->setText(note ? "本文暂无标题" : "打开一篇笔记后自动生成大纲");
    if (note && editorSessions_.contains(note->id)) {
        QList<QPair<int, QTreeWidgetItem *>> parents;
        for (const auto &heading: editorSessions_[note->id].headings) {
            while (!parents.isEmpty() && parents.last().first >= heading.level) parents.removeLast();
            auto *item = new QTreeWidgetItem;
            item->setText(0, heading.title);
            item->setData(0, Qt::UserRole, heading.line);
            if (parents.isEmpty()) outline_->addTopLevelItem(item);
            else parents.last().second->addChild(item);
            parents.append({heading.level, item});
        }
        outline_->expandAll();
    }
    outlineHint_->setVisible(outline_->topLevelItemCount() == 0);
}

void MainWindow::updateStatus() {
    const auto *note = store_.note(currentNoteId());
    if (!note) return;
    status_->setText(QStringLiteral("%1 字 · %2 行 · %3").arg(note->content.size()).arg(note->content.count('\n') + 1).arg(viewMode_ == ViewMode::Split ? "分屏" : previewModeEnabled_ ? "预览" : "编辑中"));
}

void MainWindow::updatePanels() {
    if (!left_ || !right_) return;
    siteConfig_->setText(width() < 600 ? "配置" : "站点配置");
    sitePreview_->setText(width() < 600 ? "预览" : "站点预览");
    const auto *book = store_.notebook(currentNotebookId_);
    export_->setText(width() < 600 ? "导出" : book && book->type == "site" ? "导出站点"
                                                                           : "导出电子书");
    findChild<QLabel *>("appTitle")->setVisible(width() >= 420);
    const int unit = fontMetrics().horizontalAdvance('M');
    left_->setVisible(leftRequested_ && width() >= unit * 52);
    right_->setVisible(rightRequested_ && width() >= unit * 80);
}

void MainWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);
    updatePanels();
}

bool MainWindow::save() {
    if (!storageReady_) {
        QMessageBox::warning(this, "无法保存", "原有笔记数据读取失败，为避免覆盖，当前修改尚未写入磁盘。");
        return false;
    }
    QString error;
    if (!store_.save(&error)) {
        QMessageBox::warning(this, "无法保存笔记", error);
        return false;
    }
    for (int index = 0; index < tabs_->count(); ++index) {
        if (const auto *note = store_.note(tabs_->tabData(index).toString())) {
            tabs_->setTabText(index, note->title);
            tabs_->setTabToolTip(index, note->title);
            tabs_->setTabIcon(index, {});
        }
    }
    return true;
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (!storageReady_ || save()) event->accept();
    else
        event->ignore();
}

void MainWindow::createNotebook() {
    OverlayDialog dialog(this, "newNotebookDialog");
    auto *layout = column(&dialog, 22, 12);
    auto *title = label("新建笔记本", &dialog);
    title->setStyleSheet("font-size: 15px; font-weight: 600;");
    layout->addWidget(title);
    auto *name = new QLineEdit(&dialog);
    name->setObjectName("notebookName");
    name->setPlaceholderText("笔记本名称");
    auto *type = new QComboBox(&dialog);
    type->setObjectName("notebookType");
    type->addItem("普通笔记本（目录 + 元数据文件）", "normal");
    type->addItem("静态站点（Gridea 式站点配置）", "site");
    type->addItem("电子书（GitBook 式 SUMMARY.md）", "ebook");
    QString locationRoot = NotebookStore::defaultRoot();
    auto *location = new QLineEdit(&dialog);
    location->setObjectName("notebookLocation");
    location->setReadOnly(true);
    location->setMinimumWidth(0);
    auto *locationRow = new QWidget(&dialog);
    auto *locationLayout = row(locationRow, 0, 0, 6);
    locationLayout->addWidget(location, 1);
    auto *browse = button("浏览…", "browseLocation", locationRow);
    locationLayout->addWidget(browse);
    const auto updateLocation = [&locationRoot, name, location] {
        const auto title = name->text().trimmed();
        location->setText(locationRoot + "/" + NotebookStore::sanitized(title.isEmpty() ? QStringLiteral("未命名笔记本") : title));
    };
    updateLocation();
    connect(name, &QLineEdit::textChanged, location, updateLocation);
    connect(browse, &QPushButton::clicked, &dialog, [&locationRoot, &dialog, updateLocation] {
        const auto chosen = QFileDialog::getExistingDirectory(&dialog, "选择笔记本保存目录", locationRoot);
        if (!chosen.isEmpty()) {
            locationRoot = chosen;
            updateLocation();
        }
    });
    for (const auto &field: QList<QPair<QString, QWidget *>>{{"名称", name}, {"类型", type}, {"保存位置", locationRow}}) {
        auto *caption = label(field.first, &dialog, true);
        caption->setBuddy(field.second);
        layout->addWidget(caption);
        layout->addWidget(field.second);
    }
    layout->addStretch();
    auto *actions = new QWidget(&dialog);
    auto *actionsLayout = row(actions, 0, 0, 8);
    actionsLayout->addStretch();
    auto *cancel = button("取消", "cancelNotebook", actions);
    auto *create = button("创建", "confirmNotebook", actions);
    create->setProperty("primary", true);
    create->setDefault(true);
    actionsLayout->addWidget(cancel);
    actionsLayout->addWidget(create);
    layout->addWidget(actions);
    connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(create, &QPushButton::clicked, &dialog, &QDialog::accept);
    name->setFocus();
    if (dialog.exec() != QDialog::Accepted) return;
    Notebook book;
    book.id = NotebookStore::newId();
    book.name = name->text().trimmed().isEmpty() ? QStringLiteral("未命名笔记本") : name->text().trimmed();
    book.type = type->currentData().toString();
    book.path = NotebookStore::uniqueDirectory(locationRoot, book.name);
    if (!QDir().mkpath(book.path)) {
        QMessageBox::warning(this, "无法创建笔记本", "无法创建目录：" + book.path);
        return;
    }
    if (book.type == "site") {
        book.config = {{"title", book.name}, {"theme", "minimal"}, {"footer", "Powered by MDNote"}, {"commentPlatform", "none"}, {"deployPlatform", "github-pages"}, {"branch", "gh-pages"}};
    }
    currentNotebookId_ = book.id;
    store_.notebooks.append(book);
    save();
    refreshNotebooks();
}

void MainWindow::openNotebook() {
    const auto directory = QFileDialog::getExistingDirectory(this, "打开笔记本", NotebookStore::defaultRoot());
    if (directory.isEmpty()) return;
    const QString path = QDir(directory).absolutePath();
    for (const auto &book: store_.notebooks) {
        if (QDir(book.path).absolutePath() == path) {
            currentNotebookId_ = book.id;
            search_->clear();
            refreshNotebooks();
            return;
        }
    }
    Notebook book;
    QString error;
    if (!NotebookStore::readNotebook(path, book, &error)) {
        QMessageBox::warning(this, "无法打开笔记本", error);
        return;
    }
    currentNotebookId_ = book.id;
    store_.notebooks.append(book);
    save();
    search_->clear();
    refreshNotebooks();
}

void MainWindow::closeNotebook() {
    const auto *book = store_.notebook(currentNotebookId_);
    if (!book) return;
    const auto hint = QStringLiteral("关闭笔记本「%1」？\n关闭后不再显示在列表中，磁盘上的文件会保留，之后可通过「打开笔记本」重新加入。").arg(book->name);
    if (QMessageBox::question(this, "关闭笔记本", hint, QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes) return;
    // 先落盘，保证笔记本目录中的索引包含最新元信息，供之后恢复。
    if (!save()) return;
    const QString id = book->id;
    {
        const QSignalBlocker blocker(tabs_);
        for (int index = tabs_->count() - 1; index >= 0; --index) {
            const auto noteId = tabs_->tabData(index).toString();
            const auto *owner = store_.owner(noteId);
            if (owner && owner->id == id) {
                tabs_->removeTab(index);
                removeEditor(noteId);
            }
        }
    }
    for (int index = 0; index < store_.notebooks.size(); ++index) {
        if (store_.notebooks[index].id == id) {
            store_.notebooks.removeAt(index);
            break;
        }
    }
    currentNotebookId_ = store_.notebooks.isEmpty() ? QString() : store_.notebooks.first().id;
    save();
    search_->clear();
    refreshNotebooks();
    refreshEditor();
}

void MainWindow::createNote(const QString &parentId, bool folder) {
    auto *book = store_.notebook(currentNotebookId_);
    if (!book) return;
    const auto kind = folder ? QStringLiteral("文件夹") : book->type == "site" ? QStringLiteral("文章")
                                                  : book->type == "ebook"      ? QStringLiteral("章节")
                                                                               : QStringLiteral("笔记");
    bool accepted = false;
    auto title = QInputDialog::getText(this, "新建" + kind, kind + "名称：", QLineEdit::Normal, {}, &accepted).trimmed();
    if (!accepted || title.isEmpty()) return;
    if (book->type == "normal" && !folder && !title.endsWith(".md")) title += ".md";
    Note note;
    note.id = NotebookStore::newId();
    note.parentId = parentId;
    note.title = title;
    note.folder = folder;
    if (book->type == "site") note.metadata = {{"date", QDate::currentDate().toString(Qt::ISODate)}, {"slug", "post-" + note.id.left(8)}};
    book->notes.append(note);
    save();
    refreshTree();
    if (!folder) openNote(note.id);
}

void MainWindow::showTreeMenu(const QPoint &position) {
    auto *book = store_.notebook(currentNotebookId_);
    auto *item = tree_->itemAt(position);
    const auto id = item ? item->data(0, Qt::UserRole).toString() : QString();
    auto *note = store_.note(id);
    QMenu menu(this);
    if (book && (!note || note->folder)) {
        menu.addAction(book->type == "site" ? "新建文章" : book->type == "ebook" ? "添加章节"
                                                                                 : "新建笔记",
                       this, [this, id, note] { createNote(note ? id : QString(), false); });
        if (book->type == "normal") menu.addAction("新建文件夹", this, [this, id, note] { createNote(note ? id : QString(), true); });
        if (book->type == "site") menu.addAction("站点配置", this, &MainWindow::showSiteConfig);
    }
    if (book && note && book->type == "ebook") {
        menu.addAction("添加子章节", this, [this, id] { createNote(id, false); });
        menu.addSeparator();
        QList<int> siblings;
        int current = -1;
        for (int index = 0; index < book->notes.size(); ++index) {
            if (book->notes[index].parentId != note->parentId) continue;
            if (book->notes[index].id == id) current = static_cast<int>(siblings.size());
            siblings.append(index);
        }
        for (int direction: {-1, 1}) {
            auto *action = menu.addAction(direction < 0 ? "上移" : "下移");
            const int target = current + direction;
            action->setEnabled(target >= 0 && target < siblings.size());
            connect(action, &QAction::triggered, this, [this, book, siblings, current, target] {
                book->notes.swapItemsAt(siblings[current], siblings[target]);
                save();
                refreshTree();
            });
        }
        auto *indent = menu.addAction("缩进为子章节");
        indent->setEnabled(current > 0);
        connect(indent, &QAction::triggered, this, [this, book, id, siblings, current] {
            store_.note(id)->parentId = book->notes[siblings[current - 1]].id;
            save();
            refreshTree();
        });
        auto *outdent = menu.addAction("提升层级");
        outdent->setEnabled(!note->parentId.isEmpty());
        connect(outdent, &QAction::triggered, this, [this, id] {
            auto *target = store_.note(id);
            const auto *parent = store_.note(target->parentId);
            if (parent) {
                auto *owner = store_.owner(id);
                int sourceIndex = 0;
                int parentIndex = 0;
                for (int index = 0; index < owner->notes.size(); ++index) {
                    if (owner->notes[index].id == id) sourceIndex = index;
                    if (owner->notes[index].id == parent->id) parentIndex = index;
                }
                target->parentId = parent->parentId;
                owner->notes.move(sourceIndex, sourceIndex < parentIndex ? parentIndex : parentIndex + 1);
            }
            save();
            refreshTree();
        });
    }
    if (book && note) {
        if (!menu.actions().isEmpty()) menu.addSeparator();
        menu.addAction("重命名", this, [this, id] {
            auto *target = store_.note(id);
            bool accepted = false;
            const auto title = QInputDialog::getText(this, "重命名", "名称：", QLineEdit::Normal, target->title, &accepted).trimmed();
            if (!accepted || title.isEmpty()) return;
            target->title = title;
            save();
            refreshTree();
        });
        menu.addAction("删除", this, [this, id] {
            if (QMessageBox::question(this, "删除", "确认删除此内容及其子项？该操作不可恢复。", QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes) return;
            auto *owner = store_.owner(id);
            QSet<QString> removed{id};
            bool changed = true;
            while (changed) {
                changed = false;
                for (const auto &entry: owner->notes) {
                    if (removed.contains(entry.parentId) && !removed.contains(entry.id)) {
                        removed.insert(entry.id);
                        changed = true;
                    }
                }
            }
            for (const auto &entry: owner->notes) {
                // 子孙随父目录递归删除，只需处理顶层被删项。
                if (removed.contains(entry.id) && !removed.contains(entry.parentId)) NotebookStore::removeNoteFiles(*owner, entry);
            }
            for (int index = static_cast<int>(owner->notes.size()) - 1; index >= 0; --index) {
                if (removed.contains(owner->notes[index].id)) owner->notes.removeAt(index);
            }
            {
                const QSignalBlocker blocker(tabs_);
                for (int index = tabs_->count() - 1; index >= 0; --index) {
                    const auto noteId = tabs_->tabData(index).toString();
                    if (!removed.contains(noteId)) continue;
                    tabs_->removeTab(index);
                    removeEditor(noteId);
                }
            }
            save();
            refreshEditor();
        });
    }
    if (!menu.actions().isEmpty()) menu.addSeparator();
    menu.addAction("打开笔记本…", this, &MainWindow::openNotebook);
    if (book) menu.addAction("关闭笔记本", this, &MainWindow::closeNotebook);
    menu.exec(tree_->viewport()->mapToGlobal(position));
}

void MainWindow::showSiteConfig() {
    auto *book = store_.notebook(currentNotebookId_);
    if (!book || book->type != "site") return;
    OverlayDialog dialog(this, "siteConfigDialog", true);
    auto *layout = column(&dialog);
    auto *bar = new QWidget(&dialog);
    bar->setObjectName("dialogBar");
    auto *barLayout = row(bar, 16, 11);
    barLayout->addWidget(label("站点配置", bar));
    barLayout->addStretch();
    auto *close = button("关闭", "closeSiteConfig", bar);
    barLayout->addWidget(close);
    layout->addWidget(bar);
    auto *scroll = new QScrollArea(&dialog);
    scroll->setWidgetResizable(true);
    auto *body = new QWidget(scroll);
    body->setObjectName("configBody");
    auto *form = column(body, 24, 10);
    auto heading = [&](const QString &text) {
        auto *title = label(text, body);
        title->setProperty("heading", true);
        form->addWidget(title);
    };
    auto field = [&](const QString &key, const QString &caption, const QStringList &choices = QStringList(), const QStringList &values = QStringList()) {
        auto *container = new QWidget(body);
        auto *fieldLayout = new QFormLayout(container);
        fieldLayout->setContentsMargins(0, 0, 0, 0);
        fieldLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        fieldLayout->setRowWrapPolicy(QFormLayout::WrapLongRows);
        auto *captionLabel = label(caption, container, true);
        captionLabel->setMinimumWidth(96);
        captionLabel->setWordWrap(false);
        if (choices.isEmpty()) {
            auto *input = new QLineEdit(book->config[key].toString(), container);
            input->setObjectName("config_" + key);
            input->setMinimumWidth(0);
            if (key == "token" || key == "clientSecret") input->setEchoMode(QLineEdit::Password);
            captionLabel->setBuddy(input);
            fieldLayout->addRow(captionLabel, input);
            connect(input, &QLineEdit::textEdited, &dialog, [this, book, key](const QString &value) {
                book->config[key] = value;
                saveTimer_->start();
            });
        } else {
            auto *select = new QComboBox(container);
            select->setObjectName("config_" + key);
            for (int index = 0; index < choices.size(); ++index) select->addItem(choices[index], values.value(index, choices[index]));
            select->setCurrentIndex(qMax(0, select->findData(book->config[key].toString())));
            captionLabel->setBuddy(select);
            fieldLayout->addRow(captionLabel, select);
            connect(select, &QComboBox::currentIndexChanged, &dialog, [this, book, key, select] {
                book->config[key] = select->currentData().toString();
                saveTimer_->start();
            });
        }
        form->addWidget(container);
    };
    heading("基本信息");
    field("title", "站点标题");
    field("description", "站点描述");
    field("author", "作者");
    field("domain", "域名");
    heading("评论");
    field("commentPlatform", "评论平台", {"关闭", "Gitalk", "Disqus"}, {"none", "gitalk", "disqus"});
    field("commentOwner", "GitHub 用户");
    field("commentRepo", "评论仓库");
    field("clientId", "Client ID");
    field("clientSecret", "Client Secret");
    heading("友链");
    auto *links = new QWidget(body);
    auto *linksLayout = column(links, 0, 6);
    auto appendLink = [this, book, links, linksLayout](const QJsonObject &value) {
        auto *container = new QWidget(links);
        auto *linkLayout = row(container, 0, 0, 8);
        for (const auto &key: {QStringLiteral("name"), QStringLiteral("url")}) {
            auto *input = new QLineEdit(value[key].toString(), container);
            input->setObjectName(key);
            input->setPlaceholderText(key == "name" ? "名称" : "https://");
            input->setMinimumWidth(0);
            linkLayout->addWidget(input, 1);
            connect(input, &QLineEdit::textEdited, links, [this, book, links] {
                QJsonArray entries;
                for (auto *linkRow: links->findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly)) {
                    if (linkRow->isHidden()) continue;
                    entries.append(QJsonObject{{"name", linkRow->findChild<QLineEdit *>("name")->text()}, {"url", linkRow->findChild<QLineEdit *>("url")->text()}});
                }
                book->config["links"] = entries;
                saveTimer_->start();
            });
        }
        auto *remove = button("删除", "removeLink", container);
        linkLayout->addWidget(remove);
        connect(remove, &QPushButton::clicked, links, [container] {
            container->hide();
            auto *input = container->findChild<QLineEdit *>("name");
            emit input->textEdited(input->text());
            container->deleteLater();
        });
        linksLayout->addWidget(container);
    };
    for (const auto &entry: book->config["links"].toArray()) appendLink(entry.toObject());
    form->addWidget(links);
    auto *addLink = button("＋ 添加友链", "addLink", body);
    form->addWidget(addLink, 0, Qt::AlignLeft);
    connect(addLink, &QPushButton::clicked, &dialog, [appendLink] { appendLink({}); });
    heading("主题与页脚");
    field("theme", "主题", {"minimal", "notes", "tech"});
    field("footer", "页脚文字");
    heading("发布配置");
    field("deployPlatform", "发布平台", {"GitHub Pages", "Coding Pages", "Gitee Pages"}, {"github-pages", "coding", "gitee"});
    field("repo", "仓库");
    field("branch", "分支");
    field("token", "Token");
    form->addWidget(label("配置随 config.json 导出；使用顶栏「导出站点」生成静态文件后按此配置部署。", body, true));
    form->addStretch();
    scroll->setWidget(body);
    layout->addWidget(scroll, 1);
    connect(close, &QPushButton::clicked, &dialog, &QDialog::accept);
    dialog.exec();
    save();
}

QString MainWindow::siteHtml(const Notebook &book, const QString &page) {
    const auto escape = [](const QString &value) { return value.toHtmlEscaped(); };
    const auto &config = book.config;
    QString body = "<h1>" + escape(config["title"].toString(book.name)) + "</h1><p>" + escape(config["description"].toString()) + "</p>";
    body += "<p><a href=\"index.html\">首页</a>&nbsp;&nbsp;&nbsp;<a href=\"archives.html\">归档</a>&nbsp;&nbsp;&nbsp;<a href=\"about.html\">关于</a>&nbsp;&nbsp;&nbsp;<a href=\"links.html\">友链</a></p><hr>";
    if (page == "about.html") body += "<h2>关于</h2><p>" + escape(config["author"].toString()) + "</p>";
    else if (page == "links.html") {
        body += "<h2>友链</h2><ul>";
        for (const auto &value: config["links"].toArray()) {
            const auto link = value.toObject();
            body += "<li><a href=\"" + escape(link["url"].toString()) + "\">" + escape(link["name"].toString()) + "</a></li>";
        }
        body += "</ul>";
    } else {
        bool article = false;
        for (const auto &note: book.notes) {
            if (page != "post-" + note.id + ".html") continue;
            body += markdownHtml(note.content);
            article = true;
            break;
        }
        if (!article) {
            body += page == "archives.html" ? "<h2>归档</h2>" : "";
            for (const auto &note: book.notes) {
                body += "<h3><a href=\"post-" + note.id + ".html\">" + escape(note.title) + "</a></h3><p style=\"color:#8a8f98\">" + escape(note.metadata["date"].toString()) + "　" + escape(note.metadata["category"].toString()) + "</p>";
            }
        }
    }
    body += "<hr><p style=\"color:#8a8f98\">" + escape(config["footer"].toString()) + "</p>";
    const auto theme = config["theme"].toString();
    const auto themeStyle = theme == "notes" ? QStringLiteral("body { background: #fffdf7; } h1,h2 { color: #795548; }") : theme == "tech" ? QStringLiteral("body { font-family: monospace; } h1,h2 { color: #3b6fd4; }")
                                                                                                                                           : QString();
    return R"(<!DOCTYPE html><html lang="zh-CN"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1"><title>)" + escape(config["title"].toString(book.name)) + "</title><style>" + markdownStyle + "body { max-width:760px; margin:0 auto; padding:24px; }" + themeStyle + "</style></head><body>" + body + "</body></html>";
}

void MainWindow::showSitePreview() {
    auto *book = store_.notebook(currentNotebookId_);
    if (!book || book->type != "site") return;
    OverlayDialog dialog(this, "sitePreviewDialog", true);
    dialog.setMaximumSize(size());
    dialog.resize(size());
    auto *layout = column(&dialog);
    auto *bar = new QWidget(&dialog);
    bar->setObjectName("dialogBar");
    auto *barLayout = row(bar, 14, 8, 10);
    barLayout->addWidget(label("站点预览", bar));
    barLayout->addWidget(label("由当前站点配置与文章实时生成", bar, true));
    barLayout->addStretch();
    auto *close = button("关闭", "closeSitePreview", bar);
    barLayout->addWidget(close);
    layout->addWidget(bar);
    auto *browser = new SiteBrowser(&dialog);
    browser->setObjectName("siteBrowser");
    browser->setOpenLinks(false);
    browser->setHtml(siteHtml(*book, "index.html"));
    layout->addWidget(browser, 1);
    connect(browser, &QTextBrowser::anchorClicked, &dialog, [this, book, browser](const QUrl &url) {
        if (url.isRelative()) browser->setHtml(siteHtml(*book, url.path()));
        else if (url.scheme() == "https" || url.scheme() == "http")
            QDesktopServices::openUrl(url);
    });
    connect(close, &QPushButton::clicked, &dialog, &QDialog::accept);
    dialog.exec();
}

void MainWindow::exportNotebook() {
    const auto *book = store_.notebook(currentNotebookId_);
    if (!book || book->type == "normal") return;
    const auto directory = QFileDialog::getExistingDirectory(this, "选择导出目录");
    if (directory.isEmpty()) return;
    // Export into a new directory; never overwrite a user's existing files.
    const QString destination = directory + "/qsnote-" + book->id.left(8) + "-" + QString::number(QDateTime::currentMSecsSinceEpoch());
    QMap<QString, QByteArray> files;
    if (book->type == "site") {
        for (const auto &page: {"index.html", "archives.html", "about.html", "links.html"}) files[page] = siteHtml(*book, page).toUtf8();
        for (const auto &note: book->notes) files["post-" + note.id + ".html"] = siteHtml(*book, "post-" + note.id + ".html").toUtf8();
        files["config.json"] = QJsonDocument(book->config).toJson();
    } else {
        QString summary = "# Summary\n\n";
        QString html = R"(<!DOCTYPE html><html lang="zh-CN"><head><meta charset="utf-8"><style>)" + markdownStyle + "</style></head><body><h1>" + book->name.toHtmlEscaped() + "</h1>";
        QList<QPair<QString, int>> pending{{QString(), -1}};
        while (!pending.isEmpty()) {
            const auto entry = pending.takeLast();
            if (!entry.first.isEmpty()) {
                const auto *note = store_.note(entry.first);
                const auto filename = "chapter-" + note->id + ".md";
                summary += QString(entry.second * 2, ' ') + "* [" + note->title + "](" + filename + ")\n";
                files[filename] = note->content.toUtf8();
                html += markdownHtml(note->content);
            }
            for (auto it = book->notes.crbegin(); it != book->notes.crend(); ++it) {
                if (it->parentId == entry.first) pending.append({it->id, entry.second + 1});
            }
        }
        files["SUMMARY.md"] = summary.toUtf8();
        files["book.html"] = (html + "</body></html>").toUtf8();
    }
    if (!QDir().mkpath(destination)) {
        QMessageBox::warning(this, "导出失败", "无法创建导出目录。");
        return;
    }
    for (auto entry = files.cbegin(); entry != files.cend(); ++entry) {
        QSaveFile file(destination + "/" + entry.key());
        if (!file.open(QIODevice::WriteOnly) || file.write(entry.value()) != entry.value().size() || !file.commit()) {
            QMessageBox::warning(this, "导出失败", file.errorString());
            return;
        }
    }
    status_->setText("已导出至 " + destination);
}
