#include "MainWindow.h"
#include "FlowLayout.h"
#include "MarkdownEditor.h"
#include "MarkdownPreview.h"
#include "core/StaticSite.h"
#include "core/SitePublisher.h"
#include "core/EbookExporter.h"
#include <QSettings>
#include <QStandardPaths>
#include <QWebEngineView>
#include <QProgressDialog>
#include <QTcpServer>
#include <QTcpSocket>
#include <QMimeDatabase>
#include <QScopedValueRollback>
#include <QScrollBar>
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDate>
#include <QDateEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDropEvent>
#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <vtextedit/vtextedit.h>
#include <vtextedit/vmarkdowneditor.h>
#include <vtextedit/texteditutils.h>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QScreen>
#include <QScrollArea>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStackedWidget>
#include <QStyledItemDelegate>
#include <QTabBar>
#include <QTextBlock>
#include <QWindow>
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
QString defaultExportDirectory() {
    auto documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (documents.isEmpty()) documents = QDir::homePath();
    return documents + "/qsnote_exports";
}
QIcon icon(const QString &kind) {
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.scale(2, 2);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor("#758195"), 1.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    if (kind == "left" || kind == "right") {
        painter.drawRoundedRect(QRectF(1.5, 1.5, 13, 13), 2, 2);
        painter.drawLine(QPointF(kind == "left" ? 5 : 10, 2), QPointF(kind == "left" ? 5 : 10, 14));
    } else if (kind == "search") {
        painter.drawEllipse(QRectF(2, 2, 8, 8));
        painter.drawLine(QPointF(9, 9), QPointF(13, 13));
    } else if (kind == "dirty") {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor("#3b6fd4"));
        painter.drawEllipse(QRectF(4.5, 4.5, 7, 7));
    } else if (kind == "folder") {
        painter.setBrush(QColor("#e8eef7"));
        painter.drawPolygon(QPolygonF{QPointF(1, 3), QPointF(6, 3), QPointF(8, 5), QPointF(15, 5), QPointF(15, 13), QPointF(1, 13)});
    } else {
        painter.drawRoundedRect(QRectF(3, 1, 10, 14), 1.5, 1.5);
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
        return {qBound(76, fontMetrics().horizontalAdvance(tabText(index)) + 64 + (tabIcon(index).isNull() ? 0 : 20), 200), 32};
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

// Serve generated files over loopback so Pure's scripts and search use normal HTTP URLs.
class SiteServer : public QTcpServer {
public:
    SiteServer(const QString &directory, const QString &root, QObject *parent) : QTcpServer(parent) {
        connect(this, &QTcpServer::newConnection, this, [this, directory, root] {
            while (hasPendingConnections()) {
                auto *socket = nextPendingConnection();
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
                connect(socket, &QTcpSocket::readyRead, socket, [socket, directory, root] {
                    auto request = socket->property("request").toByteArray() + socket->readAll();
                    if (request.size() > 65536) { socket->disconnectFromHost(); return; }
                    if (!request.contains("\r\n\r\n")) { socket->setProperty("request", request); return; }
                    const auto parts = request.left(request.indexOf("\r\n")).split(' ');
                    auto path = parts.size() >= 2 ? QUrl::fromEncoded(parts[1]).path() : QString();
                    const bool allowed = path.startsWith(root) && (parts[0] == "GET" || parts[0] == "HEAD");
                    path = path.mid(root.size());
                    if (path.isEmpty() || path.endsWith('/')) path += "index.html";
                    const auto absolute = QFileInfo(directory + "/" + path).canonicalFilePath();
                    const bool insideSite = allowed && absolute.startsWith(QFileInfo(directory).canonicalFilePath() + "/");
                    if (insideSite && QFileInfo(absolute).isDir()) {
                        auto url = QUrl::fromEncoded(parts[1]);
                        url.setPath(url.path() + '/');
                        socket->write("HTTP/1.1 301 Moved Permanently\r\nLocation: " + url.toEncoded() + "\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
                        socket->disconnectFromHost();
                        return;
                    }
                    QFile file(absolute);
                    const bool found = insideSite && file.open(QIODevice::ReadOnly);
                    const auto body = found ? file.readAll() : QByteArray("Not found");
                    const auto mime = found ? QMimeDatabase().mimeTypeForFile(absolute).name().toUtf8() : QByteArray("text/plain");
                    socket->write(found ? "HTTP/1.1 200 OK\r\n" : "HTTP/1.1 404 Not Found\r\n");
                    socket->write("Content-Type: " + mime + "\r\nContent-Length: " + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n");
                    if (parts[0] != "HEAD") socket->write(body);
                    socket->disconnectFromHost();
                });
            }
        });
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

}// namespace

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setWindowTitle(QCoreApplication::applicationName());
    QFile stylesheet(":/style.qss");
    if (stylesheet.open(QIODevice::ReadOnly)) setStyleSheet(QString::fromUtf8(stylesheet.readAll()));
    const auto available = screen()->availableGeometry().size();
    resize(available.width() * 4 / 5, available.height() * 4 / 5);
    buildUi();
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
        QTimer::singleShot(0, this, [this, error] { QMessageBox::warning(this, "无法读取笔记", error); });
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
    refreshEditor();
    // 标签页延迟到窗口显示后恢复：构造期间加入的标签不会触发父布局重算，标签栏会保持 0 高度不可见。
    QTimer::singleShot(0, this, [this, savedOpenNotes, savedNoteId] {
        for (const auto &id: savedOpenNotes) openNote(id);
        for (int index = 0; index < tabs_->count(); ++index) {
            if (tabs_->tabData(index).toString() == savedNoteId) {
                tabs_->setCurrentIndex(index);
                break;
            }
        }
    });
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

void MainWindow::buildUi() {
    auto *root = new QWidget(this);
    setCentralWidget(root);
    auto *layout = column(root);
    auto *header = new QWidget(root);
    header_ = header;
    header->setObjectName("header");
    header->setMinimumHeight(56);
    header->installEventFilter(this);
    auto *headerLayout = new QGridLayout(header);
    headerLayout->setContentsMargins(16, 10, 16, 10);
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
    toggleLeft_ = button({}, "toggleLeft", headerLeft);
    toggleLeft_->setProperty("iconButton", true);
    toggleLeft_->setIcon(icon("left"));
    toggleLeft_->setToolTip("收起/展开笔记本栏");
    toggleLeft_->setAccessibleName(toggleLeft_->toolTip());
    headerLeftLayout->addWidget(toggleLeft_);
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
    notebooks_->setAccessibleName("当前笔记本");
    notebooks_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    notebooks_->setMinimumContentsLength(1);
    notebooks_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    notebooks_->setMinimumWidth(240);
    notebooks_->setMaximumWidth(320);
    pickerLayout->addWidget(notebooks_, 1);
    auto *add = button("＋", "newNotebook", picker);
    add->setProperty("iconButton", true);
    add->setToolTip("新建笔记本");
    add->setAccessibleName(add->toolTip());
    pickerLayout->addWidget(add);
    headerLeftLayout->addWidget(picker);
    headerLeftLayout->addStretch(1);
    search_ = new QLineEdit(header);
    search_->setObjectName("search");
    search_->setPlaceholderText("搜索当前笔记本 (Ctrl+Shift+F)");
    search_->setAccessibleName("搜索当前笔记本");
    search_->setMinimumWidth(320);
    search_->setMaximumWidth(520);
    search_->setClearButtonEnabled(true);
    search_->addAction(icon("search"), QLineEdit::LeadingPosition);
    headerLayout->addWidget(search_, 0, 1, Qt::AlignCenter);
    siteConfig_ = button("站点配置", "siteConfig", headerRight);
    sitePreview_ = button("站点预览", "sitePreview", headerRight);
    export_ = button("导出", "exportNotebook", headerRight);
    auto *siteActions = new QWidget(headerRight);
    siteActions->setObjectName("siteActions");
    auto *siteActionsLayout = new FlowLayout(siteActions, 0, 6);
    autoNumberChapters_ = new QCheckBox("章节自动编号", siteActions);
    autoNumberChapters_->setObjectName("autoNumberChapters");
    autoNumberChapters_->setToolTip("按章节层级显示 1、1.1、1.2 等编号，同时应用于电子书导出");
    siteActionsLayout->addWidget(autoNumberChapters_);
    for (auto *action: {siteConfig_, sitePreview_, export_}) siteActionsLayout->addWidget(action);
    headerRightLayout->addWidget(siteActions);
    toggleRight_ = button({}, "toggleRight", headerRight);
    toggleRight_->setProperty("iconButton", true);
    toggleRight_->setIcon(icon("right"));
    toggleRight_->setToolTip("收起/展开右侧栏");
    toggleRight_->setAccessibleName(toggleRight_->toolTip());
    headerRightLayout->addWidget(toggleRight_);
    const QList<QPair<QString, QString>> windowButtons = {{"–", "windowMinimize"}, {"□", "windowMaximize"}, {"×", "windowClose"}};
    for (const auto &entry: windowButtons) {
        auto *windowButton = button(entry.first, entry.second, headerRight);
        windowButton->setProperty("windowButton", true);
        windowButton->setToolTip(entry.second == "windowMinimize" ? "最小化" : entry.second == "windowMaximize" ? "最大化" : "关闭");
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
    auto *leftLayout = column(left_);
    auto *leftBody = new QWidget(left_);
    auto *bodyLayout = column(leftBody, 10, 8);
    groupBy_ = new QComboBox(leftBody);
    groupBy_->setObjectName("groupBy");
    groupBy_->addItems({"按分类", "按标签", "按日期"});
    bodyLayout->addWidget(groupBy_);
    todoStatus_ = new QComboBox(leftBody);
    todoStatus_->setObjectName("todoStatus");
    todoStatus_->setAccessibleName("事项状态筛选");
    todoStatus_->addItems({"进行中", "已完成"});
    bodyLayout->addWidget(todoStatus_);
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
    tree_->setIconSize(QSize(16, 16));
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
    auto *stripLayout = row(strip, 8, 7, 0);
    tabs_ = new NoteTabBar(strip);
    tabs_->setObjectName("noteTabs");
    tabs_->setTabsClosable(true);
    tabs_->setExpanding(false);
    tabs_->setElideMode(Qt::ElideRight);
    tabs_->setUsesScrollButtons(true);
    tabs_->setDrawBase(false);
    strip->setMinimumHeight(46);
    stripLayout->addWidget(tabs_);
    centerLayout->addWidget(strip);
    toolbar_ = new QWidget(center);
    toolbar_->setObjectName("toolbar");
    buildToolbar();
    centerLayout->addWidget(toolbar_);
    pages_ = new QStackedWidget(center);
    pages_->setObjectName("editorPages");
    pages_->setMinimumSize(0, 0);
    auto *empty = new QWidget(pages_);
    empty->setObjectName("emptyState");
    auto *emptyLayout = column(empty, 12, 6);
    emptyLayout->addStretch();
    auto *emptyIcon = label({}, empty);
    emptyIcon->setPixmap(icon("ebook").pixmap(QSize(48, 48)));
    emptyIcon->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(emptyIcon);
    const QList<QPair<QString, QString>> emptyLabels = {
        {"emptyTitle", "开始记录"},
        {"emptyDescription", "从左侧选择一篇笔记开始，或新建一个笔记本"},
        {"emptyShortcut", "普通笔记 · 静态站点 · 电子书 · 待办事项"}};
    for (const auto &entry: emptyLabels) {
        auto *hint = label(entry.second, empty, true);
        hint->setObjectName(entry.first);
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

    right_ = new QWidget(splitter_);
    right_->setObjectName("rightPanel");
    auto *rightLayout = column(right_);
    auto *rightHeader = new QWidget(right_);
    rightHeader->setObjectName("panelHeader");
    auto *rightHeaderLayout = row(rightHeader, 18, 16, 6);
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
    outline_->setContentsMargins(10, 4, 10, 20);
    outline_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    outline_->setMinimumSize(0, 0);
    rightLayout->addWidget(outline_, 1);
    buildTodoUi();
    splitter_->setStretchFactor(0, 0);
    splitter_->setStretchFactor(1, 1);
    splitter_->setStretchFactor(2, 0);
    QTimer::singleShot(0, this, [this] {
        splitter_->setSizes(pages_->currentWidget() == todoListPage_ ? QList<int>{264, 640, qMax(240, width() - 904)}
                                                                  : QList<int>{264, qMax(240, width() - 496), 232});
    });
    connect(add, &QPushButton::clicked, this, &MainWindow::createNotebook);
    connect(notebooks_, &QComboBox::currentIndexChanged, this, [this] {
        currentNotebookId_ = notebooks_->currentData().toString();
        search_->clear();
        refreshNotebooks();
    });
    connect(search_, &QLineEdit::textChanged, this, &MainWindow::refreshTree);
    connect(groupBy_, &QComboBox::currentIndexChanged, this, &MainWindow::refreshTree);
    connect(todoStatus_, &QComboBox::currentIndexChanged, this, [this] { refreshTodoList(); });
    connect(tree_, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem *item) {
        const auto *book = store_.notebook(currentNotebookId_);
        if (!book || book->type != "todo" || !item) return;
        todoCategoryFilter_ = item->data(0, Qt::UserRole).toString();
        refreshTodoList();
    });
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
    connect(autoNumberChapters_, &QCheckBox::toggled, this, [this](bool checked) {
        auto *book = store_.notebook(currentNotebookId_);
        if (!book || book->type != "ebook") return;
        book->config["autoNumberChapters"] = checked;
        save();
        refreshTree();
    });
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
    auto *layout = new FlowLayout(toolbar_, 8, 3);
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
            separator->setStyleSheet("color: #e7ebf0; margin: 6px 4px;");
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
    modes->setObjectName("viewModes");
    auto *modeLayout = row(modes, 3, 3, 2);
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

void MainWindow::buildTodoUi() {
    todoListPage_ = new QWidget(pages_);
    todoListPage_->setObjectName("todoListPage");
    auto *listLayout = column(todoListPage_, 16, 12);
    auto *add = button("＋ 新建事项", "newTodo", todoListPage_);
    listLayout->addWidget(add);
    todoHint_ = label("暂无进行中的事项", todoListPage_, true);
    listLayout->addWidget(todoHint_);
    todoList_ = new QTreeWidget(todoListPage_);
    todoList_->setObjectName("todoList");
    todoList_->setHeaderHidden(true);
    todoList_->setRootIsDecorated(false);
    todoList_->setMinimumSize(0, 0);
    todoList_->setColumnCount(2);
    todoList_->header()->setStretchLastSection(false);
    todoList_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    todoList_->header()->setSectionResizeMode(1, QHeaderView::Fixed);
    todoList_->header()->resizeSection(1, 40);
    listLayout->addWidget(todoList_, 1);
    pages_->addWidget(todoListPage_);

    todoDetails_ = new QWidget(right_);
    todoDetails_->setObjectName("todoDetails");
    auto *detailsLayout = column(todoDetails_, 16, 12);
    auto *detailsTitle = label("事项详情", todoDetails_, true);
    detailsTitle->setObjectName("sectionTitle");
    detailsLayout->addWidget(detailsTitle);
    todoTitle_ = new QLineEdit(todoDetails_);
    todoTitle_->setObjectName("todoTitle");
    todoTitle_->setPlaceholderText("事项标题");
    todoTitle_->setAccessibleName("事项标题");
    detailsLayout->addWidget(todoTitle_);
    todoCompleted_ = new QCheckBox("已完成", todoDetails_);
    todoCompleted_->setObjectName("todoCompleted");
    detailsLayout->addWidget(todoCompleted_);
    todoCategory_ = new QComboBox(todoDetails_);
    todoCategory_->setObjectName("todoCategory");
    todoCategory_->setAccessibleName("事项分类");
    detailsLayout->addWidget(label("分类", todoDetails_, true));
    detailsLayout->addWidget(todoCategory_);
    todoContent_ = new QPlainTextEdit(todoDetails_);
    todoContent_->setObjectName("todoContent");
    todoContent_->setPlaceholderText("选择事项后编辑详情");
    todoContent_->setAccessibleName("事项详情正文");
    todoContent_->setMinimumSize(0, 0);
    detailsLayout->addWidget(todoContent_, 1);
    right_->layout()->addWidget(todoDetails_);
    todoDetails_->hide();

    connect(add, &QPushButton::clicked, this, &MainWindow::createTodo);
    connect(todoList_, &QTreeWidget::currentItemChanged, this, [this] { refreshTodoDetails(); });
    connect(todoTitle_, &QLineEdit::textEdited, this, [this](const QString &text) {
        auto *item = todoList_->currentItem();
        auto *note = item ? store_.note(item->data(0, Qt::UserRole).toString()) : nullptr;
        if (!note) return;
        note->title = text.trimmed().isEmpty() ? QStringLiteral("未命名事项") : text.trimmed();
        item->setText(0, note->title);
        item->setToolTip(0, note->title);
        saveTimer_->start();
    });
    connect(todoContent_, &QPlainTextEdit::textChanged, this, [this] {
        const auto *item = todoList_->currentItem();
        auto *note = item ? store_.note(item->data(0, Qt::UserRole).toString()) : nullptr;
        if (!note) return;
        note->content = todoContent_->toPlainText();
        note->contentDirty = true;
        saveTimer_->start();
    });
    connect(todoCategory_, &QComboBox::currentTextChanged, this, [this](const QString &category) {
        const auto *item = todoList_->currentItem();
        auto *note = item ? store_.note(item->data(0, Qt::UserRole).toString()) : nullptr;
        if (!note) return;
        note->metadata["category"] = category;
        save();
        refreshTodoList();
    });
    connect(todoCompleted_, &QCheckBox::toggled, this, [this](bool checked) {
        const auto *item = todoList_->currentItem();
        auto *note = item ? store_.note(item->data(0, Qt::UserRole).toString()) : nullptr;
        if (!note) return;
        note->metadata["completed"] = checked;
        save();
        refreshTodoList();
    });
    connect(todoList_, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem *item) {
        auto *note = store_.note(item->data(0, Qt::UserRole).toString());
        if (!note || note->metadata["completed"].toBool() == (item->checkState(0) == Qt::Checked)) return;
        note->metadata["completed"] = item->checkState(0) == Qt::Checked;
        save();
        // 等待复选框的鼠标事件处理完毕后再移除当前行。
        QTimer::singleShot(0, this, [this] { refreshTodoList(); });
    });
}

void MainWindow::refreshTodoCategories() {
    const auto *book = store_.notebook(currentNotebookId_);
    if (!book || book->type != "todo") return;
    if (todoNotebookId_ != book->id) {
        todoNotebookId_ = book->id;
        todoCategoryFilter_.clear();
        const QSignalBlocker blocker(todoStatus_);
        todoStatus_->setCurrentIndex(0);
    }
    QStringList categories{QStringLiteral("默认")};
    for (const auto &value: book->config["categories"].toArray()) categories << value.toString();
    for (const auto &note: book->notes) categories << note.metadata["category"].toString();
    categories.removeAll({});
    categories.removeAll(QStringLiteral("全部"));
    categories.removeDuplicates();
    const QSignalBlocker treeBlocker(tree_);
    const QSignalBlocker categoryBlocker(todoCategory_);
    tree_->clear();
    tree_->setDragDropMode(QAbstractItemView::NoDragDrop);
    groupBy_->hide();
    treeHint_->hide();
    todoCategory_->clear();
    todoCategory_->addItems(categories);
    categories.prepend(QStringLiteral("全部"));
    for (const auto &category: categories) {
        auto *item = new QTreeWidgetItem(tree_, {category});
        const auto filter = category == "全部" ? QString() : category;
        item->setData(0, Qt::UserRole, filter);
        if (filter == todoCategoryFilter_) tree_->setCurrentItem(item);
    }
    refreshTodoList();
}

void MainWindow::refreshTodoList(const QString &selectedId) {
    const auto *book = store_.notebook(currentNotebookId_);
    if (!book || book->type != "todo") return;
    const auto *current = todoList_->currentItem();
    const auto id = !selectedId.isEmpty() ? selectedId : current ? current->data(0, Qt::UserRole).toString() : QString();
    const QSignalBlocker blocker(todoList_);
    todoList_->clear();
    const auto query = search_->text().trimmed();
    for (const auto &note: book->notes) {
        if (note.folder || note.metadata["completed"].toBool() != (todoStatus_->currentIndex() == 1)) continue;
        if (!todoCategoryFilter_.isEmpty() && note.metadata["category"].toString("默认") != todoCategoryFilter_) continue;
        if (!query.isEmpty() && !note.title.contains(query, Qt::CaseInsensitive) && !note.content.contains(query, Qt::CaseInsensitive)) continue;
        auto *item = new QTreeWidgetItem(todoList_, {note.title});
        item->setData(0, Qt::UserRole, note.id);
        item->setToolTip(0, note.title);
        item->setCheckState(0, note.metadata["completed"].toBool() ? Qt::Checked : Qt::Unchecked);
        auto *remove = button("×", "todoDelete", todoList_);
        remove->setProperty("iconButton", true);
        remove->setToolTip("删除");
        remove->setAccessibleName("删除");
        connect(remove, &QPushButton::clicked, this, [this, noteId = note.id] { deleteTodo(noteId); });
        todoList_->setItemWidget(item, 1, remove);
        if (note.id == id) todoList_->setCurrentItem(item);
    }
    if (!todoList_->currentItem() && todoList_->topLevelItemCount()) todoList_->setCurrentItem(todoList_->topLevelItem(0));
    todoHint_->setText(!query.isEmpty() ? "没有匹配的事项" : todoStatus_->currentIndex() == 0 ? "暂无进行中的事项" : "暂无已完成的事项");
    todoHint_->setVisible(todoList_->topLevelItemCount() == 0);
    refreshTodoDetails();
}

void MainWindow::refreshTodoDetails() {
    const auto *item = todoList_->currentItem();
    const auto *note = item ? store_.note(item->data(0, Qt::UserRole).toString()) : nullptr;
    const QSignalBlocker titleBlocker(todoTitle_);
    const QSignalBlocker categoryBlocker(todoCategory_);
    const QSignalBlocker completedBlocker(todoCompleted_);
    const QSignalBlocker contentBlocker(todoContent_);
    todoDetails_->setEnabled(note);
    todoTitle_->setText(note ? note->title : QString());
    todoCategory_->setCurrentText(note ? note->metadata["category"].toString("默认") : QStringLiteral("默认"));
    todoCompleted_->setChecked(note && note->metadata["completed"].toBool());
    todoContent_->setPlainText(note ? note->content : QString());
}

void MainWindow::createTodo() {
    auto *book = store_.notebook(currentNotebookId_);
    if (!book || book->type != "todo") return;
    bool accepted = false;
    const auto title = QInputDialog::getText(this, "新建事项", "事项标题：", QLineEdit::Normal, {}, &accepted).trimmed();
    if (!accepted || title.isEmpty()) return;
    Note note;
    note.id = NotebookStore::newId();
    note.title = title;
    note.metadata = {{"category", todoCategoryFilter_.isEmpty() ? QStringLiteral("默认") : todoCategoryFilter_}, {"completed", false}};
    book->notes.append(note);
    save();
    search_->clear();
    todoStatus_->setCurrentIndex(0);
    refreshTodoList(note.id);
    todoTitle_->setFocus();
}

void MainWindow::deleteTodo(const QString &noteId) {
    auto *book = store_.notebook(currentNotebookId_);
    if (!book || book->type != "todo") return;
    auto *note = store_.note(noteId);
    if (!note) return;
    const auto hint = QStringLiteral("确认删除事项「%1」？该操作不可恢复。").arg(note->title);
    if (QMessageBox::question(this, "删除事项", hint, QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes) return;
    NotebookStore::removeNoteFiles(*book, *note);
    for (int index = static_cast<int>(book->notes.size()) - 1; index >= 0; --index) {
        if (book->notes[index].id == noteId) book->notes.removeAt(index);
    }
    save();
    refreshTodoList();
}

void MainWindow::createTodoCategory() {
    auto *book = store_.notebook(currentNotebookId_);
    if (!book || book->type != "todo") return;
    bool accepted = false;
    const auto name = QInputDialog::getText(this, "新建分类", "分类名称：", QLineEdit::Normal, {}, &accepted).trimmed();
    if (!accepted || name.isEmpty()) return;
    if (name == "全部" || todoCategory_->findText(name) >= 0) {
        QMessageBox::information(this, "无法新建分类", "该分类已存在，请使用其他名称。");
        return;
    }
    auto categories = book->config["categories"].toArray();
    categories.append(name);
    book->config["categories"] = categories;
    todoCategoryFilter_ = name;
    save();
    refreshTodoCategories();
}

void MainWindow::renameTodoCategory(const QString &name) {
    auto *book = store_.notebook(currentNotebookId_);
    if (!book || book->type != "todo" || name.isEmpty() || name == QStringLiteral("默认")) return;
    bool accepted = false;
    const auto newName = QInputDialog::getText(this, "重命名分类", "新的分类名称：", QLineEdit::Normal, name, &accepted).trimmed();
    if (!accepted || newName.isEmpty() || newName == name) return;
    if (newName == QStringLiteral("全部") || todoCategory_->findText(newName) >= 0) {
        QMessageBox::information(this, "无法重命名分类", "目标分类名称已存在，请使用其他名称。");
        return;
    }
    auto categories = book->config["categories"].toArray();
    for (int index = 0; index < categories.size(); ++index) {
        if (categories.at(index).toString() == name) categories.replace(index, newName);
    }
    book->config["categories"] = categories;
    for (auto &note: book->notes) {
        if (note.metadata["category"].toString() == name) note.metadata["category"] = newName;
    }
    if (todoCategoryFilter_ == name) todoCategoryFilter_ = newName;
    save();
    refreshTodoCategories();
}

void MainWindow::deleteTodoCategory(const QString &name) {
    auto *book = store_.notebook(currentNotebookId_);
    if (!book || book->type != "todo" || name.isEmpty() || name == QStringLiteral("默认")) return;
    int count = 0;
    for (const auto &note: book->notes) {
        if (note.metadata["category"].toString() == name) ++count;
    }
    const auto hint = count > 0 ? QStringLiteral("确认删除分类「%1」？其中 %2 个事项将移至「默认」分类。").arg(name).arg(count)
                                : QStringLiteral("确认删除分类「%1」？").arg(name);
    if (QMessageBox::question(this, "删除分类", hint, QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes) return;
    QJsonArray categories;
    for (const auto &value: book->config["categories"].toArray()) {
        if (value.toString() != name) categories.append(value);
    }
    book->config["categories"] = categories;
    for (auto &note: book->notes) {
        if (note.metadata["category"].toString() == name) note.metadata["category"] = QStringLiteral("默认");
    }
    if (todoCategoryFilter_ == name) todoCategoryFilter_.clear();
    save();
    refreshTodoCategories();
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
    autoNumberChapters_->setVisible(book && book->type == "ebook");
    {
        const QSignalBlocker numberingBlocker(autoNumberChapters_);
        autoNumberChapters_->setChecked(book && book->config["autoNumberChapters"].toBool());
    }
    export_->setVisible(book && (book->type == "site" || book->type == "ebook"));
    export_->setText(site ? "发布站点" : "导出电子书");
    updatePanels();
    refreshEditor();
}

void MainWindow::refreshTree() {
    const auto *currentBook = store_.notebook(currentNotebookId_);
    const bool todo = currentBook && currentBook->type == "todo";
    todoStatus_->setVisible(todo);
    if (todo) {
        refreshTodoCategories();
        return;
    }
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
    const auto titles = NotebookStore::chapterTitles(*book);
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
            item->setText(0, titles.value(note.id));
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

QString MainWindow::currentNoteId() const {
    for (const auto &book: store_.notebooks) {
        if (book.id == currentNotebookId_ && book.type == "todo") return {};
    }
    return tabs_->tabData(tabs_->currentIndex()).toString();
}

void MainWindow::openNote(const QString &id) {
    const auto *owner = store_.owner(id);
    if (owner && owner->type == "todo") return;
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
        session.menu = documentEditor->createEditorMenu(documentEditor);
        connect(session.menu, &QMenu::aboutToShow, this, [this] { setPreview(false); });
        editors_->addWidget(documentEditor);
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
    editors_->removeWidget(session.editor);
    disconnect(session.editor, nullptr, this, nullptr);
    disconnect(session.editor->getTextEdit(), nullptr, this, nullptr);
    disconnect(session.editor->getTextEdit()->verticalScrollBar(), nullptr, this, nullptr);
    delete session.editor;
}

void MainWindow::refreshEditor() {
    const auto *book = store_.notebook(currentNotebookId_);
    const bool todo = book && book->type == "todo";
    if (todo != (pages_->currentWidget() == todoListPage_)) {
        splitter_->setStretchFactor(1, todo ? 0 : 1);
        splitter_->setStretchFactor(2, todo ? 1 : 0);
        splitter_->setSizes(todo ? QList<int>{264, 640, qMax(240, width() - 904)}
                                : QList<int>{264, qMax(240, width() - 496), 232});
    }
    tabs_->parentWidget()->setVisible(!todo);
    status_->setVisible(!todo);
    todoDetails_->setVisible(todo);
    outline_->setVisible(!todo);
    right_->findChild<QWidget *>("panelHeader")->setVisible(!todo);
    for (auto *mode: {editMode_, previewMode_, splitMode_}) mode->setVisible(!todo);
    if (todo) {
        toolbar_->hide();
        outlineHint_->hide();
        previewTimer_->stop();
        pages_->setCurrentWidget(todoListPage_);
        refreshTree();
        return;
    }
    const auto id = currentNoteId();
    const auto *note = store_.note(id);
    toolbar_->setVisible(note);
    if (note) {
        activateEditor(id);
        setViewMode(viewMode_);
    } else {
        pages_->setCurrentIndex(0);
        previewTimer_->stop();
        if (preview_->revision() > 0) preview_->setMarkdown({}, {});
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
    const auto sessionMode = editorSessions_[activeEditorId_].mode;
    const int line = sessionMode == ViewMode::Preview ? editorSessions_[activeEditorId_].previewLine : markdownEditor_->getTopLine();
    const QScopedValueRollback<bool> guard(syncingPreviewScroll_, true);
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
    }
    if (mode != sessionMode) {
        editorSessions_[activeEditorId_].previewLine = line;
        // Showing or resizing the editor may scroll to its caret. Restore the
        // source line after layout without changing the retained selection.
        QTimer::singleShot(0, markdownEditor_, [this, id = activeEditorId_, mode, line] {
            if (activeEditorId_ != id || viewMode_ != mode) return;
            const QScopedValueRollback<bool> guard(syncingPreviewScroll_, true);
            if (mode != ViewMode::Preview) {
                vte::TextEditUtils::scrollBlockInPage(editor_, line, vte::TextEditUtils::PagePosition::Top);
            }
            if (mode != ViewMode::Edit) preview_->scrollToLine(line);
        });
    }
    updateStatus();
}

void MainWindow::refreshOutline() {
    const auto *book = store_.notebook(currentNotebookId_);
    if (book && book->type == "todo") return;
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
    export_->setText(book && book->type == "site" ? (width() < 600 ? "发布" : "发布站点")
                                                             : (width() < 600 ? "导出" : "导出电子书"));
    int actionWidth = 0;
    if (!autoNumberChapters_->isHidden()) actionWidth += autoNumberChapters_->sizeHint().width() + 6;
    for (auto *action: {siteConfig_, sitePreview_, export_}) {
        if (!action->isHidden()) actionWidth += action->sizeHint().width() + 6;
    }
    auto *siteActions = findChild<QWidget *>("siteActions");
    siteActions->setVisible(actionWidth > 0);
    siteActions->setMaximumWidth(qMax(0, actionWidth - 6));
    findChild<QLabel *>("appTitle")->setVisible(width() >= 420);
    auto *headerLayout = qobject_cast<QGridLayout *>(header_->layout());
    const auto *headerLeft = findChild<QWidget *>("headerLeft");
    const auto *headerRight = findChild<QWidget *>("headerRight");
    // FlowLayout's size hint is its narrowest width; reserve the full action row here.
    const int rightWidth = headerRight->sizeHint().width()
                           + (actionWidth > 0 ? qMax(0, actionWidth - 6 - qMax(siteActions->minimumWidth(), siteActions->sizeHint().width())) : 0);
    const int sideWidth = qMax(headerLeft->sizeHint().width(), rightWidth);
    const auto margins = headerLayout->contentsMargins();
    const bool compact = width() < 2 * sideWidth + search_->minimumWidth()
                                   + 2 * headerLayout->horizontalSpacing() + margins.left() + margins.right();
    siteActions->setMinimumWidth(compact ? 0 : qMax(0, actionWidth - 6));
    int searchRow, searchColumn, rowSpan, columnSpan;
    headerLayout->getItemPosition(headerLayout->indexOf(search_), &searchRow, &searchColumn, &rowSpan, &columnSpan);
    if (searchRow != (compact ? 1 : 0)) {
        headerLayout->removeWidget(search_);
        headerLayout->addWidget(search_, compact ? 1 : 0, compact ? 0 : 1, 1, compact ? 3 : 1, Qt::AlignCenter);
    }
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
        it.key()->setVisible(!isMaximized() && !isFullScreen());
    }
}

void MainWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);
    updatePanels();
    updateResizeHandles();
}

void MainWindow::toggleMaximized() { setWindowState(windowState() ^ Qt::WindowMaximized); }

void MainWindow::changeEvent(QEvent *event) {
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange) updateResizeHandles();
    if (event->type() == QEvent::WindowStateChange && maximize_) {
        maximize_->setText(isMaximized() ? "❐" : "□");
        maximize_->setToolTip(isMaximized() ? "还原" : "最大化");
        maximize_->setAccessibleName(maximize_->toolTip());
    }
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::MouseButtonPress && !isMaximized() && !isFullScreen()) {
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
    if (!storageReady_ || save()) {
        // 记住当前笔记本与全部标签页，供下次启动时恢复。
        QSettings session;
        session.setValue("session/notebookId", currentNotebookId_);
        session.setValue("session/noteId", currentNoteId());
        QStringList openNotes;
        for (int index = 0; index < tabs_->count(); ++index) openNotes << tabs_->tabData(index).toString();
        session.setValue("session/openNotes", openNotes);
        event->accept();
    } else {
        event->ignore();
    }
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
    type->addItem("静态站点（Pure）", "site");
    type->addItem("电子书（章节目录 · 多格式导出）", "ebook");
    type->addItem("Todo List（分类 · 待办事项）", "todo");
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
        book.config = {{"title", book.name}, {"theme", "pure"}, {"footer", "Powered by QSNote"}, {"commentPlatform", "none"}};
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

QStringList MainWindow::siteCategories(const Notebook &book) {
    // 分类来源：站点配置中显式新建的分类 + 文章元数据中已使用的分类；default 始终存在且排最前。
    QStringList categories{QStringLiteral("default")};
    for (const auto &value: book.config["categories"].toArray()) categories << value.toString();
    for (const auto &note: book.notes) categories << note.metadata["category"].toString();
    categories.removeAll({});
    categories.removeDuplicates();
    return categories;
}


void MainWindow::createNote(const QString &parentId, bool folder) {
    auto *book = store_.notebook(currentNotebookId_);
    if (!book) return;
    if (book->type == "site") {
        const auto noteId = NotebookStore::newId();
        QDialog dialog(this);
        dialog.setWindowTitle("新建文章");
        auto *layout = new QFormLayout(&dialog);
        auto *titleInput = new QLineEdit(&dialog);
        layout->addRow("名称：", titleInput);
        auto *categoryInput = new QComboBox(&dialog);
        categoryInput->addItems(siteCategories(*book));
        layout->addRow("分类：", categoryInput);
        auto *tagsInput = new QLineEdit(&dialog);
        tagsInput->setPlaceholderText("逗号分隔");
        layout->addRow("标签：", tagsInput);
        auto *dateInput = new QDateEdit(QDate::currentDate(), &dialog);
        dateInput->setDisplayFormat("yyyy-MM-dd");
        dateInput->setCalendarPopup(true);
        layout->addRow("日期：", dateInput);
        auto *slugInput = new QLineEdit("post-" + noteId.left(8), &dialog);
        layout->addRow("Slug：", slugInput);
        auto *draftInput = new QCheckBox("保存为草稿", &dialog);
        layout->addRow(draftInput);
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        layout->addRow(buttons);
        dialog.resize(qMin(560, screen()->availableGeometry().width() * 92 / 100), dialog.sizeHint().height());
        if (dialog.exec() != QDialog::Accepted || titleInput->text().trimmed().isEmpty()) return;
        Note note;
        note.id = noteId;
        note.parentId = parentId;
        note.title = titleInput->text().trimmed();
        note.metadata = {{"date", dateInput->date().toString(Qt::ISODate)},
                         {"slug", slugInput->text().trimmed()},
                         {"category", categoryInput->currentText()},
                         {"tags", tagsInput->text().trimmed()},
                         {"draft", draftInput->isChecked()}};
        book->notes.append(note);
        save();
        refreshTree();
        openNote(note.id);
        return;
    }
    const auto kind = folder ? QStringLiteral("文件夹") : book->type == "ebook" ? QStringLiteral("章节")
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
    if (book && book->type == "todo") {
        menu.addAction("新建分类", this, &MainWindow::createTodoCategory);
        // 「全部」与内置兜底分类「默认」不可重命名/删除。
        const auto category = item ? item->data(0, Qt::UserRole).toString() : QString();
        if (!category.isEmpty() && category != QStringLiteral("默认")) {
            menu.addAction("重命名分类", this, [this, category] { renameTodoCategory(category); });
            menu.addAction("删除分类", this, [this, category] { deleteTodoCategory(category); });
        }
        menu.addSeparator();
        menu.addAction("打开笔记本…", this, &MainWindow::openNotebook);
        menu.addAction("关闭笔记本", this, &MainWindow::closeNotebook);
        menu.exec(tree_->viewport()->mapToGlobal(position));
        return;
    }
    if (book && (!note || note->folder)) {
        menu.addAction(book->type == "site" ? "新建文章" : book->type == "ebook" ? "添加章节"
                                                                                 : "新建笔记",
                       this, [this, id, note] { createNote(note ? id : QString(), false); });
        if (book->type == "normal") menu.addAction("新建文件夹", this, [this, id, note] { createNote(note ? id : QString(), true); });
        if (book->type == "site") {
            menu.addAction("新建分类", this, [this, book] {
                bool accepted = false;
                const auto name = QInputDialog::getText(this, "新建分类", "分类名称：", QLineEdit::Normal, {}, &accepted).trimmed();
                if (!accepted || name.isEmpty()) return;
                auto categories = book->config["categories"].toArray();
                for (const auto &value: categories) {
                    if (value.toString() == name) return;
                }
                categories.append(name);
                book->config["categories"] = categories;
                save();
            });
            menu.addAction("站点配置", this, &MainWindow::showSiteConfig);
        }
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
        if (book->type == "site") {
            menu.addAction("属性更改", this, [this, book, id] {
                auto *target = store_.note(id);
                QDialog dialog(this);
                dialog.setWindowTitle("文章属性");
                auto *layout = new QFormLayout(&dialog);
                auto *titleInput = new QLineEdit(target->title, &dialog);
                layout->addRow("名称：", titleInput);
                auto *categoryInput = new QComboBox(&dialog);
                categoryInput->addItems(siteCategories(*book));
                const auto currentCategory = target->metadata["category"].toString();
                categoryInput->setCurrentIndex(qMax(0, categoryInput->findText(currentCategory)));
                layout->addRow("分类：", categoryInput);
                auto *tagsInput = new QLineEdit(target->metadata["tags"].toString(), &dialog);
                tagsInput->setPlaceholderText("逗号分隔");
                layout->addRow("标签：", tagsInput);
                auto *dateInput = new QDateEdit(QDate::fromString(target->metadata["date"].toString(), Qt::ISODate), &dialog);
                dateInput->setDisplayFormat("yyyy-MM-dd");
                dateInput->setCalendarPopup(true);
                layout->addRow("日期：", dateInput);
                auto *slugInput = new QLineEdit(target->metadata["slug"].toString(), &dialog);
                layout->addRow("Slug：", slugInput);
                auto *draftInput = new QCheckBox("保存为草稿", &dialog);
                draftInput->setChecked(target->metadata["draft"].toBool());
                layout->addRow(draftInput);
                auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
                connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
                connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
                layout->addRow(buttons);
                if (dialog.exec() != QDialog::Accepted || titleInput->text().trimmed().isEmpty()) return;
                target->title = titleInput->text().trimmed();
                target->metadata["category"] = categoryInput->currentText();
                target->metadata["tags"] = tagsInput->text().trimmed();
                target->metadata["date"] = dateInput->date().toString(Qt::ISODate);
                target->metadata["slug"] = slugInput->text().trimmed();
                target->metadata["draft"] = draftInput->isChecked();
                save();
                refreshTree();
            });
        } else {
            menu.addAction("重命名", this, [this, id] {
            auto *target = store_.note(id);
            bool accepted = false;
            const auto title = QInputDialog::getText(this, "重命名", "名称：", QLineEdit::Normal, target->title, &accepted).trimmed();
            if (!accepted || title.isEmpty()) return;
            target->title = title;
            save();
            refreshTree();
        });
        }
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
    auto *navigation = new QWidget(&dialog);
    navigation->setObjectName("siteConfigNavigation");
    auto *navigationLayout = row(navigation, 16, 10);
    auto *sections = new QTabBar(navigation);
    sections->setObjectName("siteConfigTabs");
    sections->setExpanding(false);
    sections->setUsesScrollButtons(true);
    sections->setElideMode(Qt::ElideNone);
    sections->setMinimumWidth(0);
    navigationLayout->addWidget(sections);
    layout->addWidget(navigation);
    auto *scroll = new QScrollArea(&dialog);
    scroll->setWidgetResizable(true);
    auto *body = new QWidget(scroll);
    body->setObjectName("configBody");
    auto *cards = column(body, 20, 16);
    QWidget *card = nullptr;
    QVBoxLayout *form = nullptr;
    auto section = [&](const QString &name, const QString &text) {
        card = new QWidget(body);
        card->setObjectName(name);
        card->setProperty("configCard", true);
        card->setAttribute(Qt::WA_StyledBackground, true);
        form = column(card, 18, 12);
        auto *title = label(text, card);
        title->setProperty("heading", true);
        form->addWidget(title);
        cards->addWidget(card);
        card->setVisible(sections->addTab(text) == 0);
    };
    auto field = [&](const QString &key, const QString &caption, const QStringList &choices = QStringList(), const QStringList &values = QStringList()) {
        auto *container = new QWidget(card);
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
            if (key == "clientSecret") input->setEchoMode(QLineEdit::Password);
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
        return container;
    };
    section("siteBasicCard", "基本信息");
    field("title", "站点标题");
    field("description", "站点描述");
    field("author", "作者");
    field("domain", "域名");
    section("siteCommentsCard", "评论");
    auto *comments = field("commentPlatform", "评论平台", {"关闭", "Valine", "Gitalk", "Disqus"}, {"none", "valine", "gitalk", "disqus"})->findChild<QComboBox *>();
    auto commentField = [&](const QString &key, const QString &caption, const QString &platform) {
        auto *container = field(key, caption);
        container->setVisible(comments->currentData().toString() == platform);
        connect(comments, &QComboBox::currentIndexChanged, container, [container, comments, platform] {
            container->setVisible(comments->currentData().toString() == platform);
        });
    };
    commentField("commentOwner", "GitHub 用户", "gitalk");
    commentField("commentRepo", "评论仓库", "gitalk");
    commentField("clientId", "Client ID", "gitalk");
    commentField("clientSecret", "Client Secret", "gitalk");
    commentField("disqus", "Disqus Shortname", "disqus");
    commentField("valineAppId", "Valine App ID", "valine");
    commentField("valineAppKey", "Valine App Key", "valine");
    section("siteThemeCard", "主题与页脚");
    field("theme", "站点主题", {"Pure", "Stellar"}, {"pure", "stellar"});
    field("footer", "页脚文字");
    section("siteCategoriesCard", "分类管理");
    auto *categories = new QListWidget(card);
    categories->setObjectName("siteCategories");
    categories->setMinimumHeight(120);
    categories->setMaximumHeight(180);
    categories->addItems(siteCategories(*book));
    form->addWidget(categories);
    auto *categoryActions = new QWidget(card);
    auto *categoryLayout = row(categoryActions, 0, 0, 8);
    auto *addCategory = button("新建", "addSiteCategory", categoryActions);
    auto *renameCategory = button("重命名", "renameSiteCategory", categoryActions);
    auto *deleteCategory = button("删除", "deleteSiteCategory", categoryActions);
    categoryLayout->addWidget(addCategory);
    categoryLayout->addWidget(renameCategory);
    categoryLayout->addWidget(deleteCategory);
    categoryLayout->addStretch();
    form->addWidget(categoryActions);
    form->addWidget(label("删除分类后，文章将移入 default；默认分类不可重命名或删除。", card, true));
    connect(categories, &QListWidget::currentTextChanged, &dialog, [renameCategory, deleteCategory](const QString &name) {
        const bool editable = !name.isEmpty() && name != "default";
        renameCategory->setEnabled(editable);
        deleteCategory->setEnabled(editable);
    });
    categories->setCurrentRow(0);
    const auto updateCategories = [this, book, categories](const QStringList &names, const QString &selected) {
        book->config["categories"] = QJsonArray::fromStringList(names);
        categories->clear();
        categories->addItems(siteCategories(*book));
        categories->setCurrentRow(static_cast<int>(siteCategories(*book).indexOf(selected)));
        save();
        refreshTree();
    };
    const auto editCategory = [book, categories, updateCategories, &dialog](bool rename) {
        const auto previous = rename ? categories->currentItem()->text() : QString();
        bool accepted = false;
        const auto name = QInputDialog::getText(&dialog, rename ? "重命名分类" : "新建分类", "分类名称：",
                                              QLineEdit::Normal, previous, &accepted).trimmed();
        if (!accepted || name.isEmpty() || name == previous) return;
        auto names = siteCategories(*book);
        if (names.contains(name)) {
            QMessageBox::warning(&dialog, "分类已存在", "请使用其他分类名称。");
            return;
        }
        if (rename) {
            names.replace(names.indexOf(previous), name);
            for (auto &note: book->notes) {
                if (note.metadata["category"].toString() == previous) note.metadata["category"] = name;
            }
        } else {
            names.append(name);
        }
        updateCategories(names, name);
    };
    connect(addCategory, &QPushButton::clicked, &dialog, [editCategory] { editCategory(false); });
    connect(renameCategory, &QPushButton::clicked, &dialog, [editCategory] { editCategory(true); });
    connect(deleteCategory, &QPushButton::clicked, &dialog, [book, categories, updateCategories, &dialog] {
        const auto name = categories->currentItem()->text();
        if (QMessageBox::question(&dialog, "删除分类", QString("删除分类「%1」？其中的文章将移入 default，不会删除文章。").arg(name),
                                  QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes) return;
        auto names = siteCategories(*book);
        names.removeAll(name);
        for (auto &note: book->notes) {
            if (note.metadata["category"].toString() == name) note.metadata["category"] = "default";
        }
        updateCategories(names, "default");
    });
    section("siteLinksCard", "友链");
    auto *links = new QWidget(card);
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
    auto *addLink = button("＋ 添加友链", "addLink", card);
    form->addWidget(addLink, 0, Qt::AlignLeft);
    connect(addLink, &QPushButton::clicked, &dialog, [appendLink] { appendLink({}); });
    section("sitePublishCard", "生成与发布");
    auto *method = field("publishMethod", "发布方式", {"请选择", "GitHub Pages", "SSH SCP"}, {"", "github", "scp"})->findChild<QComboBox *>();
    auto publishField = [&](const QString &key, const QString &caption, const QString &platform, const QString &placeholder) {
        auto *container = field(key, caption);
        container->findChild<QLineEdit *>()->setPlaceholderText(placeholder);
        container->setVisible(method->currentData().toString() == platform);
        connect(method, &QComboBox::currentIndexChanged, container, [container, method, platform] {
            container->setVisible(method->currentData().toString() == platform);
        });
    };
    publishField("githubRepository", "仓库地址", "github", "git@github.com:user/repo.git 或 HTTPS 地址");
    publishField("githubBranch", "发布分支", "github", "gh-pages（默认）");
    publishField("sshHost", "SSH 主机", "scp", "example.com / IP / SSH 配置别名");
    publishField("sshPort", "SSH 端口", "scp", "22（默认）");
    publishField("sshUser", "SSH 用户名", "scp", "部署用户");
    publishField("sshDirectory", "远程目录", "scp", "/var/www/site（绝对路径，不含空格）");
    publishField("sshIdentityFile", "私钥文件", "scp", "可选，填写本地完整路径；留空使用 SSH 默认配置");
    auto *publishHint = label({}, card, true);
    const auto updatePublishHint = [method, publishHint] {
        const auto platform = method->currentData().toString();
        publishHint->setText(platform == "github"
            ? "使用本机 Git 凭据或 SSH 密钥。发布分支根目录将更新为站点内容；请在 GitHub 仓库 Settings → Pages 中选择该分支的 / (root)。域名请填写实际访问地址，项目站点需包含仓库子路径。"
            : platform == "scp"
            ? "使用 SSH 密钥或 ssh-agent；请先在终端连接主机并确认指纹。上传会覆盖同名文件，保留远端其他文件。"
            : "选择发布方式后填写对应设置。预览无需发布设置。");
    };
    connect(method, &QComboBox::currentIndexChanged, publishHint, updatePublishHint);
    updatePublishHint();
    form->addWidget(publishHint);
    form->addWidget(label("内置生成器离线生成 public 目录；第三方评论服务需要联网。", card, true));
    cards->addStretch();
    scroll->setWidget(body);
    layout->addWidget(scroll, 1);
    connect(sections, &QTabBar::currentChanged, &dialog, [cards, scroll](int index) {
        for (int i = 0; i < cards->count() - 1; ++i) cards->itemAt(i)->widget()->setVisible(i == index);
        scroll->verticalScrollBar()->setValue(0);
    });
    connect(close, &QPushButton::clicked, &dialog, &QDialog::accept);
    dialog.exec();
    save();
}

bool MainWindow::generateSite(const Notebook &book) {
    if (!save()) return false;
    QProgressDialog progress("正在生成静态站点…", QString(), 0, 0, this);
    progress.setObjectName("siteGenerateDialog");
    progress.setWindowTitle("生成站点");
    progress.setWindowModality(Qt::ApplicationModal);
    progress.setCancelButton(nullptr);
    progress.setMinimumDuration(0);
    progress.show();
    QString error;
    const bool ok = StaticSite::generate(book, &error);
    progress.close();
    if (!ok) QMessageBox::warning(this, "站点生成失败", error);
    return ok;
}

void MainWindow::showSitePreview() {
    auto *book = store_.notebook(currentNotebookId_);
    if (!book || book->type != "site") return;
    if (!generateSite(*book)) return;
    OverlayDialog dialog(this, "sitePreviewDialog", true);
    SiteServer server(book->path + "/public", StaticSite::rootPath(*book), &dialog);
    if (!server.listen(QHostAddress::LocalHost)) {
        QMessageBox::warning(this, "预览失败", server.errorString());
        return;
    }
    dialog.setMaximumSize(size());
    dialog.resize(size());
    auto *layout = column(&dialog);
    auto *bar = new QWidget(&dialog);
    bar->setObjectName("dialogBar");
    auto *barLayout = row(bar, 14, 8, 10);
    barLayout->addWidget(label("站点预览", bar));
    barLayout->addWidget(label(book->config["theme"].toString() == "stellar" ? "Stellar" : "Pure", bar, true));
    barLayout->addStretch();
    auto *close = button("关闭", "closeSitePreview", bar);
    barLayout->addWidget(close);
    layout->addWidget(bar);
    auto *browser = new QWebEngineView(&dialog);
    browser->setObjectName("siteBrowser");
    layout->addWidget(browser, 1);
    QUrl url;
    url.setScheme("http");
    url.setHost("127.0.0.1");
    url.setPort(server.serverPort());
    url.setPath(StaticSite::rootPath(*book));
    browser->load(url);
    connect(close, &QPushButton::clicked, &dialog, &QDialog::accept);
    dialog.exec();
}

void MainWindow::exportNotebook() {
    const auto *book = store_.notebook(currentNotebookId_);
    if (!book || (book->type != "site" && book->type != "ebook")) return;
    if (book->type == "site") {
        QString error;
        if (!SitePublisher::validate(*book, &error)) {
            QMessageBox::warning(this, "发布设置不完整", error);
            showSiteConfig();
            return;
        }
        if (!generateSite(*book)) return;
        const auto snapshot = *book;
        QProgressDialog progress("正在发布站点…", QString(), 0, 0, this);
        progress.setObjectName("sitePublishDialog");
        progress.setWindowTitle("发布站点");
        progress.setWindowModality(Qt::ApplicationModal);
        progress.setCancelButton(nullptr);
        progress.setMinimumDuration(0);
        progress.show();
        const bool ok = SitePublisher::publish(snapshot, &error);
        progress.close();
        if (!ok) {
            status_->setText("站点发布失败");
            QMessageBox::warning(this, "站点发布失败", error);
            return;
        }
        status_->setText(snapshot.config["publishMethod"].toString() == "github"
            ? "已推送至 GitHub Pages 发布分支，等待 GitHub 部署完成"
            : "站点已通过 SSH SCP 上传");
        return;
    }
    QDialog dialog(this);
    dialog.setObjectName("ebookExportDialog");
    dialog.setWindowTitle("导出电子书");
    dialog.resize(540, 300);
    auto *layout = new QVBoxLayout(&dialog);
    auto *form = new QFormLayout;
    auto *scope = new QComboBox(&dialog);
    scope->setObjectName("exportScope");
    scope->addItem("整本电子书 · " + book->name, QString());
    const auto *note = store_.note(currentNoteId());
    if (note && !note->folder && store_.owner(note->id) == book) scope->addItem("当前文档 · " + note->title, note->id);
    auto *format = new QComboBox(&dialog);
    format->setObjectName("exportFormat");
    format->addItem("EPUB · 电子书", int(EbookExporter::Format::Epub));
    format->addItem("PDF · 打印文档", int(EbookExporter::Format::Pdf));
    format->addItem("CHM · 帮助文档", int(EbookExporter::Format::Chm));
    format->addItem("HTML · 网页", int(EbookExporter::Format::Html));
    auto *directoryRow = new QWidget(&dialog);
    auto *directoryLayout = row(directoryRow, 0, 0);
    const auto defaultDirectory = defaultExportDirectory();
    QDir().mkpath(defaultDirectory);
    auto *directory = new QLineEdit(QSettings().value("ebookExport/directory", defaultDirectory).toString(), directoryRow);
    directory->setObjectName("exportDirectory");
    auto *browse = button("浏览…", "browseExportDirectory", directoryRow);
    directoryLayout->addWidget(directory, 1);
    directoryLayout->addWidget(browse);
    connect(browse, &QPushButton::clicked, &dialog, [&] {
        const auto selected = QFileDialog::getExistingDirectory(&dialog, "选择保存目录", directory->text());
        if (!selected.isEmpty()) directory->setText(selected);
    });
    form->addRow("导出范围", scope);
    form->addRow("导出格式", format);
    form->addRow("保存目录", directoryRow);
    layout->addLayout(form);
    layout->addWidget(label("整本导出保留章节层级和顺序；当前文档仅导出该文档。已有同名文件时自动添加序号。", &dialog, true));
    auto *hint = label({}, &dialog, true);
    hint->setObjectName("exportHint");
    layout->addWidget(hint);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    auto *confirm = buttons->button(QDialogButtonBox::Save);
    confirm->setObjectName("confirmExport");
    confirm->setText("导出");
    buttons->button(QDialogButtonBox::Cancel)->setText("取消");
    const auto validate = [&] {
        const bool missingCompiler = format->currentData().toInt() == int(EbookExporter::Format::Chm) && EbookExporter::chmCompiler().isEmpty();
        const bool validDirectory = QFileInfo(directory->text().trimmed()).isDir();
        hint->setText(missingCompiler ? "CHM 需要安装 Free Pascal 的 chmcmd（Linux：fp-utils）；Windows 也可将 HTML Help Workshop 的 hhc 加入 PATH。"
                                    : !validDirectory ? "请选择已有的保存目录。" : book->notes.isEmpty() ? "电子书中还没有章节。" : QString());
        confirm->setEnabled(!missingCompiler && validDirectory && !book->notes.isEmpty());
    };
    connect(format, &QComboBox::currentIndexChanged, &dialog, validate);
    connect(directory, &QLineEdit::textChanged, &dialog, validate);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    validate();
    if (dialog.exec() != QDialog::Accepted) return;
    const auto destination = directory->text().trimmed();
    QSettings().setValue("ebookExport/directory", destination);
    // Editor changes already update store_ synchronously; export that snapshot, including unsaved edits.
    const auto snapshot = *book;
    QProgressDialog progress("正在导出电子书…", QString(), 0, 0, this);
    progress.setWindowTitle("导出电子书");
    progress.setWindowModality(Qt::ApplicationModal);
    progress.setCancelButton(nullptr);
    progress.setMinimumDuration(0);
    progress.show();
    QString error;
    const auto path = EbookExporter::exportBook(snapshot, scope->currentData().toString(),
        static_cast<EbookExporter::Format>(format->currentData().toInt()), destination, &error);
    progress.close();
    if (path.isEmpty()) QMessageBox::warning(this, "导出失败", error);
    else status_->setText("已导出至 " + path);
}
