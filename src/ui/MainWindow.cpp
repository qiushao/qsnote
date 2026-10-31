#include "MainWindow.h"
#include "EbookExportDialog.h"
#include "EditorWorkspace.h"
#include "FlowLayout.h"
#include "OverlayDialog.h"
#include "PostPropertiesDialog.h"
#include "RoundedTreeWidget.h"
#include "SettingsDialog.h"
#include "SiteConfigDialog.h"
#include "SitePreviewDialog.h"
#include "TodoWorkspace.h"
#include "UiStyle.h"
#include "core/ArticleRenderer.h"
#include "core/ChapterOperations.h"
#include "core/NotebookCategories.h"
#include "core/SitePublisher.h"
#include "core/StaticSite.h"
#include "core/UpdateChecker.h"
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QDate>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QFormLayout>
#include <QGridLayout>
#include <QInputDialog>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QProgressDialog>
#include <QPushButton>
#include <QRegularExpression>
#include <QScreen>
#include <QSettings>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStackedWidget>
#include <QStyledItemDelegate>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWindow>
#include <algorithm>
#include <functional>

namespace {
QIcon icon(const QString &kind) {
    static const QHash<QString, QString> files = {
        {"left", "panel-left"}, {"leftCollapsed", "panel-left-collapsed"},
        {"right", "panel-right"}, {"rightCollapsed", "panel-right-collapsed"},
        {"normal", "book"}, {"todo", "book"}, {"site", "book"},
    };
    const auto name = files.value(kind, kind);
    return QIcon(":/icons/" + (QFile::exists(":/icons/" + name + ".svg") ? name : QStringLiteral("book")) + ".svg");
}

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
        painter->setPen(option.state & QStyle::State_Selected ? QColor("#7560a8") : QColor("#7a7a87"));
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
        drawLine(index.data().toString(), rect.top(), option.state & QStyle::State_Selected ? QColor("#7560a8") : QColor("#34343b"), 13);
        drawLine(index.data(Qt::UserRole + 3).toString(), rect.top() + 22, QColor("#7a7a87"), 12);
        painter->restore();
    }
    [[nodiscard]] QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        auto size = QStyledItemDelegate::sizeHint(option, index);
        if (!index.data(Qt::UserRole + 2).toString().isEmpty()) size.setHeight(48);
        return size;
    }
};


class NoteTree : public RoundedTreeWidget {
public:
    using RoundedTreeWidget::RoundedTreeWidget;
    std::function<void()> orderChanged;

protected:
    void dropEvent(QDropEvent *event) override {
        QTreeWidget::dropEvent(event);
        if (event->isAccepted() && orderChanged) orderChanged();
    }
};


}// namespace

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    QSettings settings;
    leftRequested_ = settings.value("session/leftPanelExpanded", true).toBool();
    rightRequested_ = settings.value("session/rightPanelExpanded", true).toBool();
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setWindowTitle(QCoreApplication::applicationName());
    applyUiSettings();
    const auto available = screen()->availableGeometry().size();
    resize(available.width() * 4 / 5, available.height() * 4 / 5);
    buildUi();
    fileWatcher_ = new QFileSystemWatcher(this);
    auto *refreshTimer = new QTimer(this);
    refreshTimer->setSingleShot(true);
    refreshTimer->setInterval(200);
    connect(fileWatcher_, &QFileSystemWatcher::directoryChanged, refreshTimer, [refreshTimer] { refreshTimer->start(); });
    connect(fileWatcher_, &QFileSystemWatcher::fileChanged, refreshTimer, [this, refreshTimer](const QString &path) {
        for (auto &book: store_.notebooks) {
            if (book.type != "normal") continue;
            for (auto &note: book.notes) {
                if (QDir(book.path).filePath(note.path) == path) note.metadata.remove("modified");
            }
        }
        refreshTimer->start();
    });
    connect(refreshTimer, &QTimer::timeout, this, &MainWindow::refreshDirectories);
    setupTray();
    QTimer::singleShot(1500, this, [this] {
        if (QSettings().value("General/checkUpdatesOnStartup", true).toBool()) checkForUpdates();
    });
    const QList<QPair<Qt::Edges, Qt::CursorShape>> resizeEdges = {
            {Qt::LeftEdge, Qt::SizeHorCursor},
            {Qt::RightEdge, Qt::SizeHorCursor},
            {Qt::TopEdge, Qt::SizeVerCursor},
            {Qt::BottomEdge, Qt::SizeVerCursor},
            {Qt::TopEdge | Qt::LeftEdge, Qt::SizeFDiagCursor},
            {Qt::TopEdge | Qt::RightEdge, Qt::SizeBDiagCursor},
            {Qt::BottomEdge | Qt::LeftEdge, Qt::SizeBDiagCursor},
            {Qt::BottomEdge | Qt::RightEdge, Qt::SizeFDiagCursor}};
    // Transparent children above the content receive edge input even over editors and scrollbars.
    for (const auto &entry: resizeEdges) {
        auto *handle = new QWidget(this);
        handle->setCursor(entry.second);
        handle->installEventFilter(this);
        resizeHandles_.insert(handle, entry.first);
    }
    updateResizeHandles();
    QString error;
    storageReady_ = store_.load(&error);
    if (!storageReady_) {
        QTimer::singleShot(0, this, [this, error] { QMessageBox::warning(this, tr("无法读取笔记"), error); });
    }
    // 恢复上次会话；待办笔记本独立于 Markdown 标签页，其余类型优先恢复笔记所属笔记本。
    QSettings session;
    const auto savedNoteId = session.value("session/noteId").toString();
    const auto savedNotebookId = session.value("session/notebookId").toString();
    const auto savedOpenNotes = session.value("session/openNotes").toStringList();
    QString firstValidNoteId;
    for (const auto &id: savedOpenNotes) {
        if (store_.note(id)) {
            firstValidNoteId = id;
            break;
        }
    }
    const auto *savedNote = store_.note(savedNoteId);
    const auto notebookNoteId = savedNote ? savedNoteId : firstValidNoteId;
    const auto *savedBook = store_.notebook(savedNotebookId);
    if (savedBook && (savedBook->type == "todo" || notebookNoteId.isEmpty())) {
        currentNotebookId_ = savedNotebookId;
    } else if (!notebookNoteId.isEmpty()) {
        if (const auto *owner = store_.owner(notebookNoteId)) currentNotebookId_ = owner->id;
    } else if (!store_.notebooks.isEmpty()) {
        currentNotebookId_ = store_.notebooks.first().id;
    }
    refreshNotebooks();
    // 标签页延迟到窗口显示后恢复：构造期间加入的标签不会触发父布局重算，标签栏会保持 0 高度不可见。
    QTimer::singleShot(0, this, [this, savedOpenNotes, savedNoteId] {
        editorWorkspace_->restoreSession(savedOpenNotes, savedNoteId);
    });
    saveTimer_ = new QTimer(this);
    saveTimer_->setSingleShot(true);
    saveTimer_->setInterval(5000);
    connect(saveTimer_, &QTimer::timeout, this, [this] { save(); });
    auto *findAll = new QShortcut(QKeySequence("Ctrl+Shift+F"), this);
    connect(findAll, &QShortcut::activated, this, [this] {
        editorWorkspace_->setFullScreenPreview(false);
        leftRequested_ = true;
        updatePanels();
        left_->show();
        search_->setFocus();
        search_->selectAll();
    });
    auto *saveShortcut = new QShortcut(QKeySequence::Save, this);
    connect(saveShortcut, &QShortcut::activated, this, [this] { save(); });
    auto *escape = new QShortcut(QKeySequence(Qt::Key_Escape), search_);
    escape->setContext(Qt::WidgetShortcut);
    connect(escape, &QShortcut::activated, search_, &QLineEdit::clear);
}

void MainWindow::openFile(const QString &path) {
    // Open after session restoration, including files delivered by macOS during startup.
    QTimer::singleShot(0, this, [this, path] {
        if (!QFileInfo(path).isFile()) return;
        const auto *book = store_.notebook(currentNotebookId_);
        if (book && book->type == "todo") {
            currentNotebookId_.clear();
            refreshNotebooks();
        }
        editorWorkspace_->openExternalFile(path);
        if (isMinimized()) showNormal();
        show();
        raise();
        activateWindow();
    });
}

void MainWindow::buildUi() {
    auto *root = new QWidget(this);
    setCentralWidget(root);
    auto *layout = column(root);
    auto *header = new QWidget(root);
    header_ = header;
    header->setObjectName("header");
    header->setMinimumHeight(50);
    header->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    header->installEventFilter(this);
    auto *headerLayout = new QGridLayout(header);
    headerLayout->setContentsMargins(16, 7, 16, 7);
    headerLayout->setSpacing(8);
    headerLayout->setColumnStretch(0, 1);
    headerLayout->setColumnStretch(2, 1);
    auto *headerLeft = new QWidget(header);
    headerLeft->setObjectName("headerLeft");
    auto *headerLeftLayout = row(headerLeft, 0, 0, 8);
    headerLayout->addWidget(headerLeft, 0, 0);
    auto *headerRight = new QWidget(header);
    headerRight->setObjectName("headerRight");
    auto *headerRightLayout = row(headerRight, 0, 0, 8);
    headerRightLayout->addStretch(1);
    headerLayout->addWidget(headerRight, 0, 2);
    auto *settings = button({}, "settings", headerLeft);
    settings->setProperty("iconButton", true);
    settings->setIcon(icon("settings"));
    settings->setIconSize(QSize(20, 20));
    settings->setToolTip(tr("设置"));
    settings->setAccessibleName(settings->toolTip());
    headerLeftLayout->addWidget(settings);
    connect(settings, &QPushButton::clicked, this, &MainWindow::showSettings);
    auto *title = label("qsnote", headerLeft);
    title->setObjectName("appTitle");
    title->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    title->setMaximumWidth(title->sizeHint().width());
    headerLeftLayout->addWidget(title);
    auto *picker = new QWidget(headerLeft);
    picker->setObjectName("notebookPicker");
    auto *pickerLayout = row(picker, 0, 0, 0);
    notebooks_ = new QComboBox(picker);
    notebooks_->setObjectName("notebooks");
    notebooks_->setItemDelegate(new NotebookDelegate(notebooks_));
    notebooks_->setAccessibleName(tr("当前笔记本"));
    notebooks_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    notebooks_->setMinimumContentsLength(1);
    notebooks_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    notebooks_->setMinimumWidth(240);
    notebooks_->setMaximumWidth(320);
    pickerLayout->addWidget(notebooks_, 1);
    auto *add = button(QString(), "newNotebook", picker);
    add->setProperty("iconButton", true);
    add->setIcon(QIcon(":/icons/plus.svg"));
    add->setIconSize(QSize(14, 14));
    add->setToolTip(tr("新建笔记本"));
    add->setAccessibleName(add->toolTip());
    pickerLayout->addWidget(add);
    headerLeftLayout->addWidget(picker);
    headerLeftLayout->addStretch(1);
    search_ = new QLineEdit(header);
    search_->setObjectName("search");
    search_->setPlaceholderText(tr("搜索当前笔记本 (Ctrl+Shift+F)"));
    search_->setAccessibleName(tr("搜索当前笔记本"));
    search_->setMinimumWidth(320);
    search_->setMaximumWidth(520);
    search_->setClearButtonEnabled(true);
    search_->addAction(icon("search"), QLineEdit::LeadingPosition);
    headerLayout->addWidget(search_, 0, 1, Qt::AlignCenter);
    siteConfig_ = button(tr("站点配置"), "siteConfig", headerRight);
    sitePreview_ = button(tr("站点预览"), "sitePreview", headerRight);
    export_ = button(tr("导出"), "exportNotebook", headerRight);
    auto *siteActions = new QWidget(headerRight);
    siteActions->setObjectName("siteActions");
    auto *siteActionsLayout = new FlowLayout(siteActions, 0, 6);
    autoNumberChapters_ = new QCheckBox(tr("章节自动编号"), siteActions);
    autoNumberChapters_->setObjectName("autoNumberChapters");
    autoNumberChapters_->setToolTip(tr("按章节层级显示 1、1.1、1.2 等编号，同时应用于电子书导出"));
    siteActionsLayout->addWidget(autoNumberChapters_);
    for (auto *action: {siteConfig_, sitePreview_, export_}) siteActionsLayout->addWidget(action);
    headerRightLayout->addWidget(siteActions);
    toggleLeft_ = button({}, "toggleLeft", headerRight);
    toggleLeft_->setProperty("iconButton", true);
    toggleLeft_->setIcon(icon("left"));
    toggleLeft_->setIconSize(QSize(20, 20));
    toggleLeft_->setToolTip(tr("收起/展开笔记本栏"));
    toggleLeft_->setAccessibleName(toggleLeft_->toolTip());
    headerRightLayout->addWidget(toggleLeft_);
    toggleRight_ = button({}, "toggleRight", headerRight);
    toggleRight_->setProperty("iconButton", true);
    toggleRight_->setIcon(icon("right"));
    toggleRight_->setIconSize(QSize(20, 20));
    toggleRight_->setToolTip(tr("收起/展开右侧栏"));
    toggleRight_->setAccessibleName(toggleRight_->toolTip());
    headerRightLayout->addWidget(toggleRight_);
    const QList<QPair<QString, QString>> windowButtons = {{"minimize", "windowMinimize"}, {"maximize", "windowMaximize"}, {"close", "windowClose"}};
    for (const auto &entry: windowButtons) {
        auto *windowButton = button({}, entry.second, headerRight);
        windowButton->setProperty("windowButton", true);
        windowButton->setIcon(icon(entry.first));
        windowButton->setIconSize(QSize(20, 20));
        windowButton->setToolTip(entry.second == "windowMinimize" ? tr("最小化") : entry.second == "windowMaximize" ? tr("最大化") : tr("关闭"));
        windowButton->setAccessibleName(windowButton->toolTip());
        headerRightLayout->addWidget(windowButton);
        if (entry.second == "windowMinimize") connect(windowButton, &QPushButton::clicked, this, [this] { showMinimized(); });
        else if (entry.second == "windowMaximize") {
            maximize_ = windowButton;
            connect(windowButton, &QPushButton::clicked, this, &MainWindow::toggleMaximized);
        } else
            connect(windowButton, &QPushButton::clicked, this, &QWidget::close);
    }
    layout->addWidget(header);
    splitter_ = new QSplitter(Qt::Horizontal, root);
    splitter_->setObjectName("mainSplitter");
    splitter_->setHandleWidth(1);
    splitter_->setChildrenCollapsible(false);
    layout->addWidget(splitter_, 1);

    left_ = new QWidget(splitter_);
    left_->setObjectName("leftPanel");
    left_->installEventFilter(this);
    auto *leftLayout = column(left_);
    auto *leftBody = new QWidget(left_);
    auto *bodyLayout = column(leftBody, 12, 12);
    groupBy_ = new QComboBox(leftBody);
    groupBy_->setObjectName("groupBy");
    groupBy_->addItems({tr("按分类"), tr("按标签"), tr("按日期")});
    bodyLayout->addWidget(groupBy_);
    treeHint_ = label({}, leftBody, true);
    treeHint_->setContentsMargins(10, 14, 10, 0);
    bodyLayout->addWidget(treeHint_);
    auto *noteTree = new NoteTree(leftBody);
    tree_ = noteTree;
    noteTree->orderChanged = [this] {
        auto *book = store_.notebook(currentNotebookId_);
        if (!book) return;
        QTreeWidgetItemIterator it(tree_);
        while (*it) {
            auto *entry = *it;
            auto *note = store_.note(entry->data(0, Qt::UserRole).toString());
            if (note) {
                note->parentId = entry->parent() ? entry->parent()->data(0, Qt::UserRole).toString() : QString();
            }
            ++it;
        }
        save();
        refreshTree();
    };
    tree_->setObjectName("noteTree");
    tree_->setItemDelegate(new NoteDelegate(tree_));
    tree_->setHeaderHidden(true);
    tree_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    tree_->setIndentation(14);
    tree_->setIconSize(QSize(16, 16));
    tree_->setTextElideMode(Qt::ElideRight);
    tree_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    tree_->setContextMenuPolicy(Qt::CustomContextMenu);
    tree_->setMinimumSize(0, 0);
    auto *deleteTree = new QShortcut(QKeySequence(Qt::Key_Delete), tree_);
    deleteTree->setContext(Qt::WidgetShortcut);
    deleteTree->setAutoRepeat(false);
    connect(deleteTree, &QShortcut::activated, this, &MainWindow::deleteSelectedTreeItems);
    auto *copyTree = new QShortcut(QKeySequence::Copy, tree_);
    copyTree->setContext(Qt::WidgetShortcut);
    connect(copyTree, &QShortcut::activated, this, [this] {
        const auto *item = tree_->currentItem();
        if (item) copyTreeItem(item->data(0, Qt::UserRole).toString());
    });
    auto *pasteTree = new QShortcut(QKeySequence::Paste, tree_);
    pasteTree->setContext(Qt::WidgetShortcut);
    connect(pasteTree, &QShortcut::activated, this, [this] {
        const auto *item = tree_->currentItem();
        pasteTreeFiles(item ? item->data(0, Qt::UserRole).toString() : QString());
    });
    bodyLayout->addWidget(tree_, 1);
    leftLayout->addWidget(leftBody, 1);

    pages_ = new QStackedWidget(splitter_);
    pages_->setObjectName("workspaces");
    pages_->setMinimumSize(0, 0);
    editorWorkspace_ = new EditorWorkspace(store_, [this] { return save(); }, pages_);
    pages_->addWidget(editorWorkspace_);
    connect(editorWorkspace_, &EditorWorkspace::currentDocumentChanged, this, &MainWindow::refreshEditor);
    connect(editorWorkspace_, &EditorWorkspace::saveRequested, this, [this] { saveTimer_->start(); });
    connect(editorWorkspace_, &EditorWorkspace::searchRequested, this, [this] {
        leftRequested_ = true;
        updatePanels();
        left_->show();
        search_->setFocus();
        search_->selectAll();
    });
    connect(editorWorkspace_, &EditorWorkspace::notebookRequested, this, [this](const QString &id) {
        search_->clear();
        notebooks_->setCurrentIndex(notebooks_->findData(id));
    });
    connect(editorWorkspace_, &EditorWorkspace::locateRequested, this, [this](const QString &id) {
        leftRequested_ = true;
        left_->show();
        if (width() < 720) right_->hide();
        QTreeWidgetItemIterator item(tree_);
        while (*item) {
            if ((*item)->data(0, Qt::UserRole).toString() == id) {
                for (auto *parent = (*item)->parent(); parent; parent = parent->parent()) parent->setExpanded(true);
                tree_->setCurrentItem(*item);
                tree_->scrollToItem(*item);
                break;
            }
            ++item;
        }
    });
    right_ = new QWidget(splitter_);
    right_->setObjectName("rightPanel");
    right_->installEventFilter(this);
    auto *rightLayout = column(right_);
    rightLayout->addWidget(editorWorkspace_->outlinePanel());
    fullScreenOutline_ = button({}, "fullScreenOutline", this);
    fullScreenOutline_->setProperty("iconButton", true);
    fullScreenOutline_->setIconSize(QSize(20, 20));
    fullScreenOutline_->setFixedSize(32, 32);
    fullScreenOutline_->setToolTip(tr("收起/展开右侧栏"));
    fullScreenOutline_->setAccessibleName(fullScreenOutline_->toolTip());
    fullScreenOutline_->hide();
    connect(fullScreenOutline_, &QPushButton::clicked, this, [this] {
        fullScreenOutlineRequested_ = !right_->isVisible();
        updatePanels();
    });
    connect(editorWorkspace_, &EditorWorkspace::fullScreenPreviewChanged, this, &MainWindow::setFullScreenPreview);
    todoWorkspace_ = new TodoWorkspace(store_, pages_);
    pages_->addWidget(todoWorkspace_);
    bodyLayout->addWidget(todoWorkspace_->categoryPanel(), 1);
    rightLayout->addWidget(todoWorkspace_->detailsPanel());
    connect(todoWorkspace_, &TodoWorkspace::saveRequested, this, [this](bool delayed) {
        if (delayed) saveTimer_->start();
        else save();
    });
    connect(todoWorkspace_, &TodoWorkspace::removalApplied, this, &MainWindow::applyRemoval);
    connect(todoWorkspace_, &TodoWorkspace::searchCleared, this, [this] {
        const QSignalBlocker blocker(search_);
        search_->clear();
    });
    connect(todoWorkspace_, &TodoWorkspace::openNotebookRequested, this, &MainWindow::openNotebook);
    connect(todoWorkspace_, &TodoWorkspace::closeNotebookRequested, this, &MainWindow::closeNotebook);
    splitter_->setStretchFactor(0, 0);
    splitter_->setStretchFactor(1, 1);
    splitter_->setStretchFactor(2, 0);
    QTimer::singleShot(0, this, [this] {
        const auto state = QSettings().value("session/mainSplitter").toByteArray();
        if (state.isEmpty() || !splitter_->restoreState(state)) {
            splitter_->setSizes(pages_->currentWidget() == todoWorkspace_ ? QList<int>{264, 640, qMax(240, width() - 904)}
                                                                      : QList<int>{264, qMax(240, width() - 496), 232});
        }
    });
    connect(add, &QPushButton::clicked, this, &MainWindow::createNotebook);
    connect(notebooks_, &QComboBox::currentIndexChanged, this, [this] {
        currentNotebookId_ = notebooks_->currentData().toString();
        search_->clear();
        refreshNotebooks();
    });
    auto *searchTimer = new QTimer(this);
    searchTimer->setSingleShot(true);
    searchTimer->setInterval(200);
    connect(search_, &QLineEdit::textChanged, searchTimer, [searchTimer] { searchTimer->start(); });
    connect(searchTimer, &QTimer::timeout, this, [this] {
        auto *book = store_.notebook(currentNotebookId_);
        if (book && book->type == "normal" && !search_->text().trimmed().isEmpty()) {
            QString error;
            if (!NotebookStore::loadAllFiles(*book, &error)) editorWorkspace_->showStatus(error);
        }
        refreshTree();
    });
    connect(groupBy_, &QComboBox::currentIndexChanged, this, &MainWindow::refreshTree);
    connect(tree_, &QTreeWidget::customContextMenuRequested, this, &MainWindow::showTreeMenu);
    connect(tree_, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem *item) {
        const auto id = item->data(0, Qt::UserRole).toString();
        auto *note = store_.note(id);
        if (note && !note->folder) editorWorkspace_->openNote(id);
    });
    connect(tree_, &QTreeWidget::itemExpanded, this, [this](QTreeWidgetItem *item) {
        auto *book = store_.notebook(currentNotebookId_);
        const auto *note = store_.note(item->data(0, Qt::UserRole).toString());
        if (!book || book->type != "normal" || !note || !note->folder || book->loadedDirectories.contains(note->path)) return;
        QString error;
        if (!NotebookStore::loadDirectory(*book, note->path, &error)) QMessageBox::warning(this, tr("无法读取笔记"), error);
        refreshTree();
    });
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
    connect(autoNumberChapters_, &QCheckBox::toggled, this, [this](bool checked) {
        auto *book = store_.notebook(currentNotebookId_);
        if (!book || book->type != "ebook") return;
        book->config["autoNumberChapters"] = checked;
        save();
        refreshTree();
    });
}

void MainWindow::refreshNotebooks() {
    const QSignalBlocker blocker(notebooks_);
    notebooks_->clear();
    for (const auto &book: store_.notebooks) {
        notebooks_->addItem(book.name, book.id);
        notebooks_->setItemData(notebooks_->count() - 1, NotebookStore::typeLabel(book.type), Qt::UserRole + 1);
        notebooks_->setItemData(notebooks_->count() - 1, book.name + " · " + NotebookStore::typeLabel(book.type), Qt::ToolTipRole);
    }
    notebooks_->setPlaceholderText(tr("无笔记本"));
    notebooks_->setCurrentIndex(notebooks_->findData(currentNotebookId_));
    auto *book = store_.notebook(currentNotebookId_);
    if (book && book->type == "todo") rightRequested_ = true;
    const bool site = book && book->type == "site";
    siteConfig_->setVisible(site);
    sitePreview_->setVisible(site);
    autoNumberChapters_->setVisible(book && book->type == "ebook");
    {
        const QSignalBlocker numberingBlocker(autoNumberChapters_);
        autoNumberChapters_->setChecked(book && book->config["autoNumberChapters"].toBool());
    }
    export_->setVisible(book && (book->type == "normal" || book->type == "site" || book->type == "ebook"));
    export_->setText(site ? tr("发布站点") : tr("导出电子书"));
    updatePanels();
    refreshEditor();
}

void MainWindow::refreshTree() {
    watchDirectories();
    const QSignalBlocker treeBlocker(tree_);
    const auto *currentBook = store_.notebook(currentNotebookId_);
    const bool todo = currentBook && currentBook->type == "todo";
    todoWorkspace_->setNotebook(currentNotebookId_, storageReady_);
    todoWorkspace_->setSearch(search_->text());
    todoWorkspace_->categoryPanel()->setVisible(todo);
    tree_->setVisible(!todo);
    if (todo) {
        groupBy_->hide();
        treeHint_->hide();
        todoWorkspace_->refresh();
        return;
    }
    QSet<QString> collapsed;
    QSet<QString> expanded;
    QTreeWidgetItemIterator iterator(tree_);
    while (*iterator) {
        if ((*iterator)->isExpanded()) expanded.insert((*iterator)->data(0, Qt::UserRole).toString());
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
    const auto titles = NotebookStore::chapterTitles(*book);
    QFileIconProvider fileIcons;
    int matches = 0;
    for (const auto &note: book->notes) {
        if (book->type == "normal" && !QFileInfo::exists(QDir(book->path).filePath(note.path)) && !QFileInfo(QDir(book->path).filePath(note.path)).isSymLink()) continue;
        if (!query.isEmpty() && (note.folder || (!note.title.contains(query, Qt::CaseInsensitive) && !note.content.contains(query, Qt::CaseInsensitive)))) continue;
        QStringList groups{QString()};
        if (book->type == "site" && query.isEmpty()) {
            if (groupBy_->currentIndex() == 0) groups = {note.metadata["category"].toString().isEmpty() ? tr("未分类") : note.metadata["category"].toString()};
            else if (groupBy_->currentIndex() == 1) {
                groups = note.metadata["tags"].toString().split(QRegularExpression("[,，]"), Qt::SkipEmptyParts);
                for (auto &tag: groups) tag = tag.trimmed();
                groups.removeDuplicates();
                if (groups.isEmpty()) groups = {tr("无标签")};
            } else
                groups = {note.metadata["date"].toString().left(7)};
        }
        for (const auto &group: groups) {
            auto *item = new QTreeWidgetItem;
            item->setText(0, titles.value(note.id));
            if (!note.folder) item->setFlags(item->flags() & ~Qt::ItemIsDropEnabled);
            if (book->type == "normal" && note.folder && !book->loadedDirectories.contains(note.path)) item->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
            if (book->type == "normal") {
                const QFileInfo file(QDir(book->path).filePath(note.path));
                const bool markdown = file.suffix().compare("md", Qt::CaseInsensitive) == 0 || file.suffix().compare("markdown", Qt::CaseInsensitive) == 0;
                item->setIcon(0, note.folder ? icon("folder") : markdown ? icon("site")
                                                                         : fileIcons.icon(file));
            } else if (book->type != "ebook")
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
                    if (groupBy_->currentIndex() == 0) groupItem->setData(0, Qt::UserRole + 4, group);
                    nodes.insert(group, groupItem);
                }
                nodes[group]->addChild(item);
                nodes[group]->setText(0, group + QStringLiteral("（%1）").arg(nodes[group]->childCount()));
                nodes[group]->setExpanded(!collapsed.contains("group:" + group));
            } else {
                tree_->addTopLevelItem(item);
                nodes.insert(note.id, item);
            }
            if (note.id == editorWorkspace_->currentNoteId()) tree_->setCurrentItem(item);
        }
        ++matches;
    }
    if (book->type == "site" && query.isEmpty() && groupBy_->currentIndex() == 0) {
        for (const auto &category: NotebookCategories::site(*book)) {
            if (nodes.contains(category)) continue;
            auto *item = new QTreeWidgetItem(tree_, {category + QStringLiteral("（0）")});
            item->setIcon(0, icon("folder"));
            item->setData(0, Qt::UserRole, "group:" + category);
            item->setData(0, Qt::UserRole + 4, category);
        }
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
        for (auto *item: nodes) item->setExpanded(book->type == "normal" ? expanded.contains(item->data(0, Qt::UserRole).toString()) : !collapsed.contains(item->data(0, Qt::UserRole).toString()));
    }
    QTreeWidgetItemIterator active(tree_);
    while (*active) {
        if ((*active)->data(0, Qt::UserRole).toString() == editorWorkspace_->currentNoteId()) tree_->setCurrentItem(*active);
        ++active;
    }
    if (!query.isEmpty()) treeHint_->setText(tr("找到 %1 篇笔记").arg(matches));
    else
        treeHint_->setText(book->type == "site" ? tr("暂无文章，右键新建") : book->type == "ebook" ? tr("暂无章节，右键添加")
                                                                                               : tr("暂无内容，右键新建"));
    treeHint_->setVisible(!query.isEmpty() || book->notes.isEmpty());
}

void MainWindow::watchDirectories() {
    if (!fileWatcher_) return;
    QSet<QString> paths;
    for (const auto &book: store_.notebooks) {
        if (book.type != "normal") continue;
        paths.insert(book.path);
        for (const auto &note: book.notes) {
            const auto path = QDir(book.path).filePath(note.path);
            if (!QFileInfo(path).isSymLink() && (note.folder ? book.loadedDirectories.contains(note.path) : note.contentLoaded)) paths.insert(path);
        }
    }
    auto watched = fileWatcher_->directories();
    watched.append(fileWatcher_->files());
    for (const auto &path: watched) {
        if (!paths.remove(path)) fileWatcher_->removePath(path);
    }
    if (!paths.isEmpty()) fileWatcher_->addPaths(paths.values());
}

void MainWindow::refreshDirectories() {
    QSet<QString> removed;
    for (auto &book: store_.notebooks) {
        if (book.type != "normal") continue;
        QSet<QString> before;
        for (const auto &note: book.notes) before.insert(note.id);
        QString error;
        if (!NotebookStore::scanDirectory(book, &error)) {
            editorWorkspace_->showStatus(error);
            continue;
        }
        for (const auto &note: book.notes) before.remove(note.id);
        removed.unite(before);
    }
    editorWorkspace_->removeDocuments(removed);
    editorWorkspace_->saveExternalFilesAndSync();
    refreshEditor();
}

void MainWindow::refreshEditor() {
    const auto *book = store_.notebook(currentNotebookId_);
    const bool todo = book && book->type == "todo";
    if (todo != (pages_->currentWidget() == todoWorkspace_)) {
        splitter_->setStretchFactor(1, todo ? 0 : 1);
        splitter_->setStretchFactor(2, todo ? 1 : 0);
        splitter_->setSizes(todo ? QList<int>{264, 640, qMax(240, width() - 904)}
                                : QList<int>{264, qMax(240, width() - 496), 232});
    }
    pages_->setCurrentWidget(todo ? static_cast<QWidget *>(todoWorkspace_) : editorWorkspace_);
    todoWorkspace_->detailsPanel()->setVisible(todo);
    editorWorkspace_->outlinePanel()->setVisible(!todo);
    editorWorkspace_->refresh(todo);
    refreshTree();
}

void MainWindow::setFullScreenPreview(bool enabled) {
    fullScreenPreview_ = enabled;
    if (enabled) {
        previousWindowState_ = windowState();
        previousGeometry_ = geometry();
        previousSplitterState_ = splitter_->saveState();
        previousLeftVisible_ = !left_->isHidden();
        previousRightVisible_ = !right_->isHidden();
        fullScreenOutlineRequested_ = previousRightVisible_;
        header_->hide();
        showFullScreen();
    } else {
        header_->show();
        setWindowState(previousWindowState_);
        setGeometry(previousGeometry_);
    }
    fullScreenOutline_->setVisible(enabled);
    updatePanels();
    if (!enabled) {
        left_->setVisible(previousLeftVisible_);
        right_->setVisible(previousRightVisible_);
        splitter_->restoreState(previousSplitterState_);
    }
    updateResizeHandles();
}

void MainWindow::updatePanels() {
    if (!left_ || !right_) return;
    if (fullScreenPreview_) {
        left_->hide();
        right_->setVisible(fullScreenOutlineRequested_);
        fullScreenOutline_->setIcon(icon(fullScreenOutlineRequested_ ? "right" : "rightCollapsed"));
        fullScreenOutline_->move(width() - fullScreenOutline_->width() - 8, 8);
        fullScreenOutline_->raise();
        return;
    }
    siteConfig_->setText(width() < 600 ? tr("配置") : tr("站点配置"));
    sitePreview_->setText(width() < 600 ? tr("预览") : tr("站点预览"));
    const auto *book = store_.notebook(currentNotebookId_);
    export_->setText(book && book->type == "site" ? (width() < 600 ? tr("发布") : tr("发布站点"))
                                                             : (width() < 600 ? tr("导出") : tr("导出电子书")));
    int actionWidth = 0;
    if (!autoNumberChapters_->isHidden()) actionWidth += autoNumberChapters_->sizeHint().width() + 6;
    for (auto *action: {siteConfig_, sitePreview_, export_}) {
        if (!action->isHidden()) actionWidth += action->sizeHint().width() + 6;
    }
    auto *siteActions = findChild<QWidget *>("siteActions");
    siteActions->setVisible(actionWidth > 0);
    siteActions->setMaximumWidth(qMax(0, actionWidth - 6));
    findChild<QLabel *>("appTitle")->setVisible(width() >= 420);
    notebooks_->setMinimumWidth(qBound(100, width() / 4, 240));
    auto *headerLayout = qobject_cast<QGridLayout *>(header_->layout());
    auto *headerLeft = findChild<QWidget *>("headerLeft");
    auto *headerRight = findChild<QWidget *>("headerRight");
    // FlowLayout's size hint is its narrowest width; reserve the full action row here.
    const int rightWidth = headerRight->sizeHint().width()
                           + (actionWidth > 0 ? qMax(0, actionWidth - 6 - qMax(siteActions->minimumWidth(), siteActions->sizeHint().width())) : 0);
    const int sideWidth = qMax(headerLeft->sizeHint().width(), rightWidth);
    const auto margins = headerLayout->contentsMargins();
    const bool compact = width() < 2 * sideWidth + search_->minimumWidth()
                                   + 2 * headerLayout->horizontalSpacing() + margins.left() + margins.right();
    siteActions->setMinimumWidth(compact ? 0 : qMax(0, actionWidth - 6));
    const bool stacked = headerLeft->minimumSizeHint().width() + headerRight->minimumSizeHint().width()
                         + margins.left() + margins.right() + 2 * headerLayout->horizontalSpacing() > width();
    int rightRow, rightColumn, rightRowSpan, rightColumnSpan;
    headerLayout->getItemPosition(headerLayout->indexOf(headerRight), &rightRow, &rightColumn, &rightRowSpan, &rightColumnSpan);
    if (rightRow != (stacked ? 2 : 0)) {
        headerLayout->removeWidget(headerLeft);
        headerLayout->removeWidget(headerRight);
        headerLayout->addWidget(headerLeft, 0, 0, 1, stacked ? 3 : 1);
        headerLayout->addWidget(headerRight, stacked ? 2 : 0, stacked ? 0 : 2, 1, stacked ? 3 : 1);
    }
    int searchRow, searchColumn, rowSpan, columnSpan;
    headerLayout->getItemPosition(headerLayout->indexOf(search_), &searchRow, &searchColumn, &rowSpan, &columnSpan);
    if (searchRow != (compact ? 1 : 0)) {
        headerLayout->removeWidget(search_);
        headerLayout->addWidget(search_, compact ? 1 : 0, compact ? 0 : 1, 1, compact ? 3 : 1, Qt::AlignCenter);
    }
    headerLayout->activate();
    const int unit = fontMetrics().horizontalAdvance('M');
    left_->setVisible(leftRequested_ && width() >= unit * 52);
    right_->setVisible(rightRequested_ && width() >= unit * 80);
}

void MainWindow::updateResizeHandles() {
    const int margin = 6;
    for (auto it = resizeHandles_.cbegin(); it != resizeHandles_.cend(); ++it) {
        const auto edges = it.value();
        const bool horizontal = edges.testFlag(Qt::LeftEdge) || edges.testFlag(Qt::RightEdge);
        const bool vertical = edges.testFlag(Qt::TopEdge) || edges.testFlag(Qt::BottomEdge);
        const int x = edges.testFlag(Qt::LeftEdge) ? 0 : edges.testFlag(Qt::RightEdge) ? width() - margin : margin;
        const int y = edges.testFlag(Qt::TopEdge) ? 0 : edges.testFlag(Qt::BottomEdge) ? height() - margin : margin;
        it.key()->setGeometry(x, y, horizontal ? margin : width() - 2 * margin, vertical ? margin : height() - 2 * margin);
        it.key()->setVisible(!maximized_ && !isFullScreen());
    }
}

void MainWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);
    updatePanels();
    updateResizeHandles();
}

void MainWindow::maximizeWindow() {
    const auto available = screen()->availableGeometry();
    normalGeometry_ = QRect(available.x() + available.width() / 10, available.y() + available.height() / 10,
                            available.width() * 4 / 5, available.height() * 4 / 5);
    maximized_ = true;
    setWindowState(windowState() | Qt::WindowMaximized);
    setGeometry(available);
    updateMaximizeButton();
}

void MainWindow::toggleMaximized() {
    if (maximized_) {
        maximized_ = false;
        setWindowState(windowState() & ~Qt::WindowMaximized);
        setGeometry(normalGeometry_);
    } else {
        normalGeometry_ = geometry();
        maximized_ = true;
        setWindowState(windowState() | Qt::WindowMaximized);
        setGeometry(screen()->availableGeometry());
    }
    updateResizeHandles();
    updateMaximizeButton();
}

void MainWindow::updateMaximizeButton() {
    if (!maximize_) return;
    maximize_->setIcon(icon(maximized_ ? "restore" : "maximize"));
    maximize_->setToolTip(maximized_ ? tr("还原") : tr("最大化"));
    maximize_->setAccessibleName(maximize_->toolTip());
}

void MainWindow::changeEvent(QEvent *event) {
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange) {
        updateResizeHandles();
        updateMaximizeButton();
    }
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::ShowToParent || event->type() == QEvent::HideToParent) {
        if (watched == left_) toggleLeft_->setIcon(icon(left_->isHidden() ? "leftCollapsed" : "left"));
        if (watched == right_) toggleRight_->setIcon(icon(right_->isHidden() ? "rightCollapsed" : "right"));
    }
    if (event->type() == QEvent::MouseButtonPress && !maximized_ && !isFullScreen()) {
        const auto edges = resizeHandles_.value(qobject_cast<QWidget *>(watched));
        auto *mouse = dynamic_cast<QMouseEvent *>(event);
        if (edges && mouse && mouse->button() == Qt::LeftButton && windowHandle()) {
            return windowHandle()->startSystemResize(edges);
        }
    }
    if (watched == header_) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto *mouse = dynamic_cast<QMouseEvent *>(event);
            if (mouse && mouse->button() == Qt::LeftButton && windowHandle() && windowHandle()->startSystemMove()) return true;
        } else if (event->type() == QEvent::MouseButtonDblClick) {
            auto *mouse = dynamic_cast<QMouseEvent *>(event);
            if (mouse && mouse->button() == Qt::LeftButton) {
                toggleMaximized();
                return true;
            }
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

bool MainWindow::save() {
    if (!storageReady_) {
        QMessageBox::warning(this, tr("无法保存"), tr("原有笔记数据读取失败，为避免覆盖，当前修改尚未写入磁盘。"));
        return false;
    }
    QString error;
    if (!store_.save(&error)) {
        QMessageBox::warning(this, tr("无法保存笔记"), error);
        return false;
    }
    return editorWorkspace_->saveExternalFilesAndSync();
}

void MainWindow::applyRemoval(const NotebookStore::RemovalResult &result) {
    editorWorkspace_->removeDocuments(result.removedIds);
    if (result.saved) editorWorkspace_->saveExternalFilesAndSync();
    if (!result.error.isEmpty()) QMessageBox::warning(this, tr("删除"), result.error);
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (!storageReady_ || save()) {
        editorWorkspace_->setFullScreenPreview(false);
        // 记住当前笔记本与全部标签页，供下次启动时恢复。
        QSettings session;
        session.setValue("session/notebookId", currentNotebookId_);
        const auto currentId = editorWorkspace_->currentNoteId();
        const auto *currentBook = store_.owner(currentId);
        const auto *currentNote = store_.note(currentId);
        session.setValue("session/noteId", currentBook && currentBook->type == "normal" && currentNote
                                                   ? QUrl::fromLocalFile(QDir(currentBook->path).filePath(currentNote->path)).toString()
                                                   : currentId);
        session.setValue("session/openNotes", editorWorkspace_->openDocuments());
        session.setValue("session/leftPanelExpanded", leftRequested_);
        session.setValue("session/rightPanelExpanded", rightRequested_);
        session.setValue("session/mainSplitter", splitter_->saveState());
        if (!quitting_ && session.value("General/minimizeToTray", false).toBool() && QSystemTrayIcon::isSystemTrayAvailable()) {
            tray_->show();
            hide();
            event->ignore();
        } else {
            tray_->hide();
            event->accept();
        }
    } else {
        event->ignore();
    }
}

void MainWindow::applyUiSettings() {
    QFile stylesheet(":/style.qss");
    if (!stylesheet.open(QIODevice::ReadOnly)) return;
    auto style = QString::fromUtf8(stylesheet.readAll());
    QSettings settings;
    auto family = settings.value("General/uiFontFamily").toString();
    if (!family.isEmpty()) {
        family.replace('\\', "\\\\");
        family.replace('"', "\\\"");
        style.replace(QRegularExpression("font-family:[^;]+;"), "font-family: \"" + family + "\";");
    }
    const int size = qBound(10, settings.value("General/uiFontSize", 14).toInt(), 24);
    const QRegularExpression sizes("font-size: (\\d+)px;");
    auto matches = sizes.globalMatch(style);
    QString scaled;
    qsizetype offset = 0;
    while (matches.hasNext()) {
        const auto match = matches.next();
        scaled += style.mid(offset, match.capturedStart() - offset);
        scaled += QStringLiteral("font-size: %1px;").arg(qRound(match.captured(1).toInt() * size / 14.0));
        offset = match.capturedEnd();
    }
    setStyleSheet(scaled + style.mid(offset));
    if (auto *title = findChild<QLabel *>("appTitle")) title->setMaximumWidth(title->sizeHint().width());
    updatePanels();
}

void MainWindow::setupTray() {
    tray_ = new QSystemTrayIcon(QIcon(":/icons/qsnote.png"), this);
    tray_->setObjectName("systemTray");
    tray_->setToolTip("qsnote");
    auto *menu = new QMenu(this);
    const auto restore = [this] {
        if (isMinimized()) showNormal();
        else show();
        raise();
        activateWindow();
    };
    menu->addAction(tr("显示主窗口"), this, restore);
    menu->addSeparator();
    auto *quit = menu->addAction(tr("退出"), this, [this] {
        quitting_ = true;
        if (close()) QApplication::quit();
        else quitting_ = false;
    });
    quit->setObjectName("quitApplication");
    tray_->setContextMenu(menu);
    connect(tray_, &QSystemTrayIcon::activated, this, [restore](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) restore();
    });
    tray_->setVisible(QSettings().value("General/minimizeToTray", false).toBool() && QSystemTrayIcon::isSystemTrayAvailable());
}

void MainWindow::checkForUpdates(QLabel *status) {
    if (!updateChecker_) updateChecker_ = new UpdateChecker(this);
    if (status) {
        status->setText(tr("正在检查更新…"));
        connect(updateChecker_, &UpdateChecker::finished, status, [status](const QString &version, const QString &error) {
            status->setText(!error.isEmpty() ? error : version.isEmpty() ? tr("当前已是最新版本。") : tr("发现新版本 %1，可前往发布页下载。").arg(version));
        }, Qt::SingleShotConnection);
    } else {
        connect(updateChecker_, &UpdateChecker::finished, this, [this](const QString &version, const QString &) {
            if (version.isEmpty() || !QSettings().value("General/checkUpdatesOnStartup", true).toBool()) return;
            auto *notice = new QMessageBox(QMessageBox::Information, tr("发现新版本"), tr("qsnote %1 已发布。").arg(version), QMessageBox::Close, this);
            notice->setAttribute(Qt::WA_DeleteOnClose);
            auto *download = notice->addButton(tr("前往下载"), QMessageBox::ActionRole);
            connect(download, &QPushButton::clicked, notice, [] {
                QDesktopServices::openUrl(QUrl("https://github.com/qiushao/qsnote/releases/latest"));
            });
            notice->open();
        }, Qt::SingleShotConnection);
    }
    updateChecker_->check();
}

void MainWindow::showSettings() {
    SettingsDialog dialog(editorWorkspace_->activeEditor(), this);
    connect(&dialog, &SettingsDialog::updateCheckRequested, this, &MainWindow::checkForUpdates);
    if (dialog.exec() != QDialog::Accepted) return;
    applyUiSettings();
    if (!editorWorkspace_->activeEditor()) editorWorkspace_->reloadSettings();
    tray_->setVisible(QSettings().value("General/minimizeToTray", false).toBool() && QSystemTrayIcon::isSystemTrayAvailable());
}

void MainWindow::createNotebook() {
    OverlayDialog dialog(this, "newNotebookDialog");
    auto *layout = column(&dialog, 20, 12);
    auto *title = label(tr("新建笔记本"), &dialog);
    title->setProperty("dialogTitle", true);
    layout->addWidget(title);
    auto *name = new QLineEdit(&dialog);
    name->setObjectName("notebookName");
    name->setPlaceholderText(tr("笔记本名称"));
    auto *type = new QComboBox(&dialog);
    type->setObjectName("notebookType");
    type->addItem(tr("普通笔记本（目录 + 元数据文件）"), "normal");
    type->addItem(tr("静态站点（Pure）"), "site");
    type->addItem(tr("电子书（章节目录 · 多格式导出）"), "ebook");
    type->addItem(tr("Todo List（分类 · 待办事项）"), "todo");
    QString locationRoot = NotebookStore::defaultRoot();
    auto *location = new QLineEdit(&dialog);
    location->setObjectName("notebookLocation");
    location->setReadOnly(true);
    location->setMinimumWidth(0);
    auto *locationRow = new QWidget(&dialog);
    auto *locationLayout = row(locationRow, 0, 0, 6);
    locationLayout->addWidget(location, 1);
    auto *browse = button(tr("浏览…"), "browseLocation", locationRow);
    locationLayout->addWidget(browse);
    const auto updateLocation = [&locationRoot, name, location] {
        const auto title = name->text().trimmed();
        location->setText(locationRoot + "/" + NotebookStore::sanitized(title.isEmpty() ? tr("未命名笔记本") : title));
    };
    updateLocation();
    connect(name, &QLineEdit::textChanged, location, updateLocation);
    connect(browse, &QPushButton::clicked, &dialog, [&locationRoot, &dialog, updateLocation] {
        const auto chosen = QFileDialog::getExistingDirectory(&dialog, tr("选择笔记本保存目录"), locationRoot);
        if (!chosen.isEmpty()) {
            locationRoot = chosen;
            updateLocation();
        }
    });
    for (const auto &field: QList<QPair<QString, QWidget *>>{{tr("名称"), name}, {tr("类型"), type}, {tr("保存位置"), locationRow}}) {
        auto *caption = label(field.first, &dialog, true);
        caption->setBuddy(field.second);
        layout->addWidget(caption);
        layout->addWidget(field.second);
    }
    layout->addStretch();
    auto *actions = new QWidget(&dialog);
    auto *actionsLayout = row(actions, 0, 0, 8);
    actionsLayout->addStretch();
    auto *cancel = button(tr("取消"), "cancelNotebook", actions);
    auto *create = button(tr("创建"), "confirmNotebook", actions);
    setPrimaryButton(create);
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
    book.name = name->text().trimmed().isEmpty() ? tr("未命名笔记本") : name->text().trimmed();
    book.type = type->currentData().toString();
    book.path = NotebookStore::uniqueDirectory(locationRoot, book.name);
    if (!QDir().mkpath(book.path)) {
        QMessageBox::warning(this, tr("无法创建笔记本"), tr("无法创建目录：") + book.path);
        return;
    }
    if (book.type == "site") {
        book.config = {{"title", book.name}, {"theme", "pure"}, {"footer", "Powered by QSNote"}, {"commentPlatform", "none"}};
    }
    currentNotebookId_ = book.id;
    store_.notebooks.append(book);
    save();
    refreshNotebooks();
}

void MainWindow::openNotebook() {
    const auto directory = QFileDialog::getExistingDirectory(this, tr("打开笔记本"), NotebookStore::defaultRoot());
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
        QMessageBox::warning(this, tr("无法打开笔记本"), error);
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
    const auto hint = tr("关闭笔记本「%1」？\n关闭后不再显示在列表中，磁盘上的文件会保留，之后可通过「打开笔记本」重新加入。").arg(book->name);
    if (QMessageBox::question(this, tr("关闭笔记本"), hint, QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes) return;
    // 先落盘，保证笔记本目录中的索引包含最新元信息，供之后恢复。
    if (!save()) return;
    ArticleRenderer(book->path, book->type == "site" ? "sites" : "notebooks").clearCache();
    const QString id = book->id;
    QSet<QString> noteIds;
    for (const auto &note: book->notes) noteIds.insert(note.id);
    editorWorkspace_->removeDocuments(noteIds);
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
}

void MainWindow::createNote(const QString &parentId, bool folder, const QString &category) {
    auto *book = store_.notebook(currentNotebookId_);
    if (!book) return;
    if (book->type == "site") {
        const auto noteId = NotebookStore::newId();
        const QJsonObject metadata{{"date", QDate::currentDate().toString(Qt::ISODate)},
                                   {"slug", "post-" + noteId.left(8)}, {"category", category}};
        PostPropertiesDialog dialog({}, metadata, NotebookCategories::site(*book), false, this);
        if (dialog.exec() != QDialog::Accepted || dialog.title().isEmpty()) return;
        Note note;
        note.id = noteId;
        note.parentId = parentId;
        note.title = dialog.title();
        note.metadata = dialog.metadata();
        book->notes.append(note);
        save();
        refreshTree();
        editorWorkspace_->openNote(note.id, true);
        return;
    }
    const auto kind = folder ? tr("文件夹") : book->type == "ebook" ? tr("章节")
                                                                                : tr("笔记");
    bool accepted = false;
    auto title = QInputDialog::getText(this, tr("新建%1").arg(kind), tr("%1名称：").arg(kind), QLineEdit::Normal, {}, &accepted).trimmed();
    if (!accepted || title.isEmpty()) return;
    if (book->type == "normal" && !folder && !title.endsWith(".md")) title += ".md";
    Note note;
    note.id = NotebookStore::newId();
    note.parentId = parentId;
    note.title = title;
    note.folder = folder;
    book->notes.append(note);
    save();
    refreshTree();
    if (!folder) {
        editorWorkspace_->openNote(note.id, true);
    }
}

void MainWindow::copyTreeItem(const QString &id) {
    const auto *book = store_.owner(id);
    if (!book || book->type != "normal" || !save()) return;
    const auto *note = store_.note(id);
    if (!note) return;
    auto *mime = new QMimeData;
    mime->setUrls({QUrl::fromLocalFile(QDir(book->path).absoluteFilePath(note->path))});
    QApplication::clipboard()->setMimeData(mime);
}

void MainWindow::pasteTreeFiles(const QString &id) {
    const auto *book = store_.notebook(currentNotebookId_);
    if (!book || book->type != "normal") return;
    const auto *mime = QApplication::clipboard()->mimeData();
    if (!mime) return;
    QStringList paths;
    for (const auto &url: mime->urls()) {
        if (url.isLocalFile()) paths.append(url.toLocalFile());
    }
    paths.removeDuplicates();
    if (paths.isEmpty() || !save()) return;
    const auto *note = store_.note(id);
    const auto parentId = note ? (note->folder ? note->id : note->parentId) : QString();
    QString error;
    const bool imported = store_.importFiles(currentNotebookId_, parentId, paths, &error);
    // A batch may have copied earlier entries before encountering an error.
    const bool saved = save();
    search_->clear();
    refreshTree();
    tree_->setCurrentItem(nullptr);
    QTreeWidgetItemIterator item(tree_);
    while (*item) {
        if ((*item)->data(0, Qt::UserRole).toString() == parentId) {
            tree_->setCurrentItem(*item);
            (*item)->setExpanded(true);
            break;
        }
        ++item;
    }
    if (!imported) QMessageBox::warning(this, tr("粘贴失败"), error);
    else if (saved)
        editorWorkspace_->showStatus(tr("已粘贴 %1 项").arg(paths.size()));
}

void MainWindow::deleteSelectedTreeItems() {
    const auto notebookId = currentNotebookId_;
    QSet<QString> ids;
    QStringList names;
    for (const auto *item: tree_->selectedItems()) {
        const auto id = item->data(0, Qt::UserRole).toString();
        const auto *note = store_.note(id);
        if (!note || ids.contains(id)) continue;
        ids.insert(id);
        names.append(note->title);
    }
    if (ids.isEmpty()) return;
    const auto message = ids.size() == 1 ? tr("确认删除此内容及其子项？该操作不可恢复。")
                                         : tr("确认删除选中的 %1 项内容及其子项？该操作不可恢复。").arg(ids.size());
    QMessageBox confirmation(QMessageBox::Question, tr("删除"), message, QMessageBox::Yes | QMessageBox::Cancel, this);
    confirmation.setTextFormat(Qt::PlainText);
    confirmation.setInformativeText(names.join('\n'));
    confirmation.setDefaultButton(QMessageBox::Cancel);
    if (confirmation.exec() != QMessageBox::Yes) return;
    if (!save()) return;
    applyRemoval(store_.removeNotes(notebookId, ids));
    refreshEditor();
}

void MainWindow::showTreeMenu(const QPoint &position) {
    auto *book = store_.notebook(currentNotebookId_);
    auto *item = tree_->itemAt(position);
    if (item && !item->isSelected()) tree_->setCurrentItem(item);
    const auto id = item ? item->data(0, Qt::UserRole).toString() : QString();
    auto *note = store_.note(id);
    QMenu menu(this);
    if (book && (!note || note->folder)) {
        const auto category = item ? item->data(0, Qt::UserRole + 4).toString() : QString();
        menu.addAction(book->type == "site" ? tr("新建文章") : book->type == "ebook" ? tr("添加章节")
                                                                                 : tr("新建笔记"),
                       this, [this, id, note, category] { createNote(note ? id : QString(), false, category); });
        if (book->type == "normal") menu.addAction(tr("新建文件夹"), this, [this, id, note] { createNote(note ? id : QString(), true); });
        if (book->type == "site") {
            menu.addAction(tr("新建分类"), this, [this, book] {
                bool accepted = false;
                const auto name = QInputDialog::getText(this, tr("新建分类"), tr("分类名称："), QLineEdit::Normal, {}, &accepted).trimmed();
                if (!accepted || name.isEmpty()) return;
                QString error;
                if (!NotebookCategories::create(*book, name, &error)) {
                    QMessageBox::warning(this, tr("分类已存在"), error);
                    return;
                }
                save();
                refreshTree();
            });
            menu.addAction(tr("站点配置"), this, &MainWindow::showSiteConfig);
        }
    }
    if (book && note && book->type == "ebook") {
        menu.addAction(tr("添加子章节"), this, [this, id] { createNote(id, false); });
        menu.addSeparator();
        const auto addMove = [this, book, id, &menu](const QString &title, ChapterOperations::Move movement) {
            auto *action = menu.addAction(title);
            action->setEnabled(ChapterOperations::canMove(*book, id, movement));
            connect(action, &QAction::triggered, this, [this, book, id, movement] {
                if (!ChapterOperations::move(*book, id, movement)) return;
                save();
                refreshTree();
            });
        };
        addMove(tr("上移"), ChapterOperations::Move::Up);
        addMove(tr("下移"), ChapterOperations::Move::Down);
        addMove(tr("缩进为子章节"), ChapterOperations::Move::Indent);
        addMove(tr("提升层级"), ChapterOperations::Move::Outdent);
    }
    if (book && note) {
        if (!menu.actions().isEmpty()) menu.addSeparator();
        const auto path = note->path.isEmpty() ? QString() : QDir(book->path).absoluteFilePath(note->path);
        auto *copyRelativePath = menu.addAction(tr("复制相对路径"), this, [relativePath = note->path] {
            QApplication::clipboard()->setText(QDir::toNativeSeparators(relativePath));
        });
        copyRelativePath->setEnabled(!path.isEmpty());
        auto *copyPath = menu.addAction(tr("复制绝对路径"), this, [path] {
            QApplication::clipboard()->setText(QDir::toNativeSeparators(path));
        });
        copyPath->setEnabled(!path.isEmpty());
        auto *openPath = menu.addAction(tr("打开路径"), this, [path, folder = note->folder] {
            QDesktopServices::openUrl(QUrl::fromLocalFile(folder ? path : QFileInfo(path).absolutePath()));
        });
        openPath->setEnabled(!path.isEmpty());
        menu.addSeparator();
        if (book->type == "site") {
            menu.addAction(tr("属性更改"), this, [this, book, id] {
                auto *target = store_.note(id);
                PostPropertiesDialog dialog(target->title, target->metadata, NotebookCategories::site(*book), true, this);
                if (dialog.exec() != QDialog::Accepted || dialog.title().isEmpty()) return;
                target->title = dialog.title();
                target->metadata = dialog.metadata();
                save();
                refreshTree();
            });
        } else {
            menu.addAction(tr("重命名"), this, [this, id] {
            auto *target = store_.note(id);
            bool accepted = false;
            const auto title = QInputDialog::getText(this, tr("重命名"), tr("名称："), QLineEdit::Normal, target->title, &accepted).trimmed();
            if (!accepted || title.isEmpty()) return;
            target->title = title;
            save();
            refreshEditor();
        });
        }
        menu.addAction(tr("删除"), this, &MainWindow::deleteSelectedTreeItems);
    }
    if (book && book->type == "normal") {
        if (!menu.actions().isEmpty()) menu.addSeparator();
        if (note) menu.addAction(tr("复制"), this, [this, id] { copyTreeItem(id); });
        auto *paste = menu.addAction(tr("粘贴"), this, [this, id] { pasteTreeFiles(id); });
        const auto *mime = QApplication::clipboard()->mimeData();
        const auto urls = mime ? mime->urls() : QList<QUrl>();
        paste->setEnabled(std::any_of(urls.cbegin(), urls.cend(), [](const QUrl &url) { return url.isLocalFile(); }));
    }
    if (!menu.actions().isEmpty()) menu.addSeparator();
    menu.addAction(tr("打开笔记本…"), this, &MainWindow::openNotebook);
    if (book) menu.addAction(tr("关闭笔记本"), this, &MainWindow::closeNotebook);
    menu.exec(tree_->viewport()->mapToGlobal(position));
}

void MainWindow::showSiteConfig() {
    auto *book = store_.notebook(currentNotebookId_);
    if (!book || book->type != "site") return;
    SiteConfigDialog dialog(*book, this);
    connect(&dialog, &SiteConfigDialog::saveRequested, this, [this](bool delayed) {
        if (delayed) saveTimer_->start();
        else save();
    });
    connect(&dialog, &SiteConfigDialog::categoriesChanged, this, &MainWindow::refreshTree);
    dialog.exec();
    save();
}

bool MainWindow::generateSite(const Notebook &book) {
    if (!save()) return false;
    QProgressDialog progress(tr("正在生成静态站点…"), QString(), 0, 0, this);
    progress.setObjectName("siteGenerateDialog");
    progress.setWindowTitle(tr("生成站点"));
    progress.setWindowModality(Qt::ApplicationModal);
    progress.setCancelButton(nullptr);
    progress.setMinimumDuration(0);
    progress.show();
    QString error;
    const bool ok = StaticSite::generate(book, &error);
    progress.close();
    if (!ok) QMessageBox::warning(this, tr("站点生成失败"), error);
    return ok;
}

void MainWindow::showSitePreview() {
    auto *book = store_.notebook(currentNotebookId_);
    if (!book || book->type != "site") return;
    if (!generateSite(*book)) return;
    SitePreviewDialog dialog(*book, this);
    QString error;
    if (!dialog.start(&error)) {
        QMessageBox::warning(this, tr("预览失败"), error);
        return;
    }
    dialog.exec();
}

void MainWindow::exportNotebook() {
    const auto *book = store_.notebook(currentNotebookId_);
    if (!book) return;
    if (book->type == "site") {
        publishSite(*book);
    } else if (book->type == "normal" || book->type == "ebook") {
        if (!save()) return;
        auto snapshot = *book;
        QString error;
        if (snapshot.type == "normal" && !NotebookStore::loadAllFiles(snapshot, &error)) {
            QMessageBox::warning(this, tr("无法读取笔记"), error);
            return;
        }
        EbookExportDialog dialog(snapshot, editorWorkspace_->currentNoteId(), this);
        const auto path = dialog.run();
        if (!path.isEmpty()) editorWorkspace_->showStatus(tr("已导出至 ") + path);
    }
}

void MainWindow::publishSite(const Notebook &book) {
    QString error;
    if (!SitePublisher::validate(book, &error)) {
        QMessageBox::warning(this, tr("发布设置不完整"), error);
        showSiteConfig();
        return;
    }
    if (!generateSite(book)) return;
    const auto snapshot = book; // NOLINT(performance-unnecessary-copy-initialization): Publishing processes events; retain the snapshot.
    QProgressDialog progress(tr("正在发布站点…"), QString(), 0, 0, this);
    progress.setObjectName("sitePublishDialog");
    progress.setWindowTitle(tr("发布站点"));
    progress.setWindowModality(Qt::ApplicationModal);
    progress.setCancelButton(nullptr);
    progress.setMinimumDuration(0);
    progress.show();
    const bool ok = SitePublisher::publish(snapshot, &error);
    progress.close();
    if (!ok) {
        editorWorkspace_->showStatus(tr("站点发布失败"));
        QMessageBox::warning(this, tr("站点发布失败"), error);
        return;
    }
    editorWorkspace_->showStatus(snapshot.config["publishMethod"].toString() == "github"
                                         ? tr("已推送至 GitHub Pages 发布分支，等待 GitHub 部署完成")
                                         : tr("站点已通过 SSH SCP 上传"));
}
