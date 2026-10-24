#include "EditorWorkspace.h"
#include "FlowLayout.h"
#include "MarkdownEditor.h"
#include "MarkdownPreview.h"
#include "UiStyle.h"
#include <QApplication>
#include <QButtonGroup>
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QDropEvent>
#include <QFileInfo>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QMimeDatabase>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScopedValueRollback>
#include <QScrollBar>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStackedWidget>
#include <QStringDecoder>
#include <QTabBar>
#include <QTextBlock>
#include <QTimer>
#include <QTreeWidget>
#include <vtextedit/previewmgr.h>
#include <vtextedit/texteditutils.h>
#include <vtextedit/vmarkdowneditor.h>
#include <vtextedit/vtextedit.h>

namespace {
QIcon icon(const QString &name) { return QIcon(":/icons/" + name + ".svg"); }
class NoteTabBar : public QTabBar {
public:
    using QTabBar::QTabBar;
    [[nodiscard]] QSize tabSizeHint(int index) const override {
        return {qBound(76, fontMetrics().horizontalAdvance(tabText(index)) + 64 + (tabIcon(index).isNull() ? 0 : 20), 200), 32};
    }

protected:
    void tabInserted(int index) override {
        QTabBar::tabInserted(index);
        const auto side = static_cast<QTabBar::ButtonPosition>(style()->styleHint(QStyle::SH_TabBar_CloseButtonPosition, nullptr, this));
        auto *close = button(QString(), "noteTabClose", this);
        close->setProperty("iconButton", true);
        close->setIcon(QIcon(":/icons/close.svg"));
        close->setIconSize(QSize(12, 12));
        close->setFixedSize(20, 20);
        close->setFocusPolicy(Qt::NoFocus);
        close->setToolTip(QCoreApplication::translate("MainWindow", "关闭标签"));
        close->setAccessibleName(close->toolTip());
        setTabButton(index, side, close);
        connect(close, &QPushButton::clicked, this, [this, close, side] {
            for (int tab = 0; tab < count(); ++tab) {
                if (tabButton(tab, side) != close) continue;
                emit tabCloseRequested(tab);
                return;
            }
        });
    }
};

}// namespace

EditorWorkspace::EditorWorkspace(NotebookStore &store, std::function<bool()> save, QWidget *parent)
    : QWidget(parent), store_(store), save_(std::move(save)) {
    auto *center = this;
    center->setObjectName("center");
    center->setMinimumWidth(0);
    auto *centerLayout = column(center);
    auto *strip = new QWidget(center);
    strip->setObjectName("tabStrip");
    auto *stripLayout = row(strip, 8, 7, 0);
    tabs_ = new NoteTabBar(strip);
    tabs_->setObjectName("noteTabs");
    tabs_->setTabsClosable(true);
    tabs_->setMovable(true);
    tabs_->setExpanding(false);
    tabs_->setElideMode(Qt::ElideRight);
    tabs_->setUsesScrollButtons(true);
    tabs_->setDrawBase(false);
    tabs_->setContextMenuPolicy(Qt::CustomContextMenu);
    strip->setMinimumHeight(46);
    stripLayout->addWidget(tabs_);
    centerLayout->addWidget(strip);
    toolbar_ = new QWidget(center);
    toolbar_->setObjectName("toolbar");
    buildToolbar();
    centerLayout->addWidget(toolbar_);
    pages_ = new QStackedWidget(center);
    pages_->setObjectName("editorPages");
    pages_->setAcceptDrops(true);
    pages_->installEventFilter(this);
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
            {"emptyTitle", QCoreApplication::translate("MainWindow", "开始记录")},
            {"emptyDescription", QCoreApplication::translate("MainWindow", "从左侧选择一篇笔记开始，或新建一个笔记本")},
            {"emptyShortcut", QCoreApplication::translate("MainWindow", "普通笔记 · 静态站点 · 电子书 · 待办事项")}};
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
    connect(preview_, &MarkdownPreview::localLinkActivated, this, &EditorWorkspace::openLocalLink);
    editorSplitter_->addWidget(preview_);
    preview_->hide();
    previewTimer_ = new QTimer(this);
    previewTimer_->setSingleShot(true);
    previewTimer_->setInterval(180);
    connect(previewTimer_, &QTimer::timeout, this, [this] {
        if (const auto *note = documentNote(currentNoteId())) preview_->setMarkdown(note->content, note->id, noteDirectory(note->id));
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

    outlinePanel_ = new QWidget(this);
    outlinePanel_->setObjectName("outlinePanel");
    auto *rightLayout = column(outlinePanel_);
    auto *rightHeader = new QWidget(outlinePanel_);
    rightHeader->setObjectName("panelHeader");
    auto *rightHeaderLayout = row(rightHeader, 18, 16, 6);
    auto *outlineTitle = label(QCoreApplication::translate("MainWindow", "大纲"), rightHeader, true);
    outlineTitle->setObjectName("sectionTitle");
    rightHeaderLayout->addWidget(outlineTitle);
    rightLayout->addWidget(rightHeader);
    outlineHint_ = label(QCoreApplication::translate("MainWindow", "打开一篇笔记后自动生成大纲"), outlinePanel_, true);
    outlineHint_->setContentsMargins(18, 20, 18, 0);
    rightLayout->addWidget(outlineHint_);
    outline_ = new QTreeWidget(outlinePanel_);
    outline_->setObjectName("outline");
    outline_->setHeaderHidden(true);
    outline_->setRootIsDecorated(false);
    outline_->setIndentation(14);
    outline_->setContentsMargins(10, 4, 10, 20);
    outline_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    outline_->setMinimumSize(0, 0);
    rightLayout->addWidget(outline_, 1);
    connect(outline_, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem *item) {
        const int line = item->data(0, Qt::UserRole).toInt();
        if (viewMode_ == ViewMode::Preview) {
            preview_->scrollToLine(line);
        } else {
            editor_->setTextCursor(QTextCursor(editor_->document()->findBlockByNumber(line)));
            editor_->ensureCursorVisible();
            if (viewMode_ == ViewMode::Split) preview_->scrollToLine(line);
            editor_->setFocus();
        }
    });
    auto *find = new QShortcut(QKeySequence::Find, window());
    connect(find, &QShortcut::activated, this, [this] {
        if (documentNote(currentNoteId())) {
            auto *focus = QApplication::focusWidget();
            if (viewMode_ == ViewMode::Preview || (viewMode_ == ViewMode::Split && focus && (focus == preview_ || preview_->isAncestorOf(focus)))) {
                preview_->showFind();
                return;
            }
            markdownEditor_->showFindReplace();
            return;
        }
        emit searchRequested();
    });
    auto *replace = new QShortcut(QKeySequence::Replace, window());
    connect(replace, &QShortcut::activated, this, [this] {
        if (!documentNote(currentNoteId())) return;
        if (viewMode_ == ViewMode::Preview) setPreview(false);
        markdownEditor_->showFindReplace();
    });
    auto *mode = new QShortcut(QKeySequence("Ctrl+E"), window());
    connect(mode, &QShortcut::activated, this, [this] { setPreview(viewMode_ != ViewMode::Preview); });
    connect(tabs_, &QTabBar::currentChanged, this, &EditorWorkspace::currentDocumentChanged);
    connect(tabs_, &QTabBar::tabCloseRequested, this, &EditorWorkspace::closeTab);
    connect(tabs_, &QTabBar::customContextMenuRequested, this, &EditorWorkspace::showTabMenu);
}

void EditorWorkspace::buildToolbar() {
    auto *layout = new FlowLayout(toolbar_, 8, 3);
    layout->setContentsMargins(8, 1, 8, 1);
    using Type = vte::TypeAction;
    struct Tool {
        QString icon;
        QString tip;
        Type action = Type::TypeBold;
        QVariant data;
    };

    const auto addSeparator = [&] {
        auto *separator = new QFrame(toolbar_);
        separator->setFrameShape(QFrame::VLine);
        separator->setStyleSheet("color: #e7ebf0; margin: 6px 4px;");
        separator->setFixedSize(11, 24);
        layout->addWidget(separator);
    };

    undo_ = button({}, "toolundo", toolbar_);
    redo_ = button({}, "toolredo", toolbar_);
    for (auto *action: {undo_, redo_}) {
        const bool undo = action == undo_;
        action->setProperty("tool", true);
        action->setIcon(icon(undo ? "undo" : "redo"));
        action->setIconSize(QSize(24, 24));
        action->setToolTip(undo ? QCoreApplication::translate("MainWindow", "撤销") : QCoreApplication::translate("MainWindow", "重做"));
        action->setFocusPolicy(Qt::NoFocus);
        action->setEnabled(false);
        layout->addWidget(action);
        connect(action, &QPushButton::clicked, this, [this, undo] {
            if (!documentNote(currentNoteId())) return;
            if (viewMode_ == ViewMode::Preview) setPreview(false);
            if (undo) editor_->undo();
            else
                editor_->redo();
            editor_->setFocus();
        });
    }
    addSeparator();

    auto *heading = button({}, "heading", toolbar_);
    heading->setProperty("tool", true);
    heading->setIcon(icon("heading"));
    heading->setIconSize(QSize(24, 24));
    heading->setToolTip(QCoreApplication::translate("MainWindow", "标题"));
    heading->setFocusPolicy(Qt::NoFocus);
    auto *headingMenu = new QMenu(heading);
    for (int level = 1; level <= 5; ++level) {
        headingMenu->addAction(QString("H%1").arg(level), this, [this, level] {
            if (!documentNote(currentNoteId())) return;
            setPreview(false);
            markdownEditor_->type(Type::TypeHeading, level);
        });
    }
    heading->setMenu(headingMenu);
    layout->addWidget(heading);
    addSeparator();

    const QList<Tool> tools = {
            {"bold", QCoreApplication::translate("MainWindow", "加粗"), Type::TypeBold},
            {"mark", QCoreApplication::translate("MainWindow", "高亮"), Type::TypeMark},
            {},
            {"link", QCoreApplication::translate("MainWindow", "链接"), Type::TypeLink},
            {"image", QCoreApplication::translate("MainWindow", "图片"), Type::TypeImage},
            {},
            {"list", QCoreApplication::translate("MainWindow", "无序列表"), Type::TypeUnorderedList},
            {"ordered", QCoreApplication::translate("MainWindow", "有序列表"), Type::TypeOrderedList},
            {"task", QCoreApplication::translate("MainWindow", "任务列表"), Type::TypeTodoList, false},
            {"quote", QCoreApplication::translate("MainWindow", "引用"), Type::TypeQuote},
            {},
            {"code", QCoreApplication::translate("MainWindow", "行内代码"), Type::TypeCode},
            {"codeblock", QCoreApplication::translate("MainWindow", "代码块"), Type::TypeCodeBlock},
            {"math", QCoreApplication::translate("MainWindow", "行内公式"), Type::TypeMath},
            {"mathblock", QCoreApplication::translate("MainWindow", "公式块"), Type::TypeMathBlock},
            {"table", QCoreApplication::translate("MainWindow", "表格"), Type::TypeTable}};
    for (const auto &tool: tools) {
        if (tool.icon.isEmpty()) {
            addSeparator();
            continue;
        }
        auto *action = button({}, "tool" + tool.icon, toolbar_);
        action->setProperty("tool", true);
        action->setIcon(icon(tool.icon));
        action->setIconSize(QSize(24, 24));
        action->setToolTip(tool.tip);
        action->setFocusPolicy(Qt::NoFocus);
        layout->addWidget(action);
        connect(action, &QPushButton::clicked, this, [this, tool] {
            if (!documentNote(currentNoteId())) return;
            setPreview(false);
            markdownEditor_->type(tool.action, tool.data);
        });
    }
    auto *more = button({}, "editorOptions", toolbar_);
    more->setProperty("tool", true);
    more->setIcon(icon("more"));
    more->setIconSize(QSize(24, 24));
    more->setToolTip(QCoreApplication::translate("MainWindow", "更多 Markdown 语法"));
    more->setFocusPolicy(Qt::NoFocus);
    auto *diagramMenu = new QMenu(more);
    struct Diagram {
        QString title;
        QString source;
        QString language = "mermaid";
    };
    const QList<Diagram> diagrams = {
            {QCoreApplication::translate("MainWindow", "流程图"), "flowchart TD\n    A[开始] --> B{是否完成？}\n    B -->|是| C[结束]\n    B -->|否| D[继续处理]\n    D --> B"},
            {QCoreApplication::translate("MainWindow", "时序图"), "sequenceDiagram\n    participant U as 用户\n    participant S as 系统\n    U->>S: 发送请求\n    S-->>U: 返回结果"},
            {QCoreApplication::translate("MainWindow", "类图"), "classDiagram\n    Animal <|-- Cat\n    class Animal {\n        +String name\n        +eat()\n    }\n    class Cat {\n        +meow()\n    }"},
            {QCoreApplication::translate("MainWindow", "甘特图"), "gantt\n    title 项目计划\n    dateFormat YYYY-MM-DD\n    section 开发\n    需求分析 :a, 2026-01-01, 2d\n    功能实现 :b, after a, 3d\n    测试验收 :after b, 2d"},
            {QCoreApplication::translate("MainWindow", "饼图"), "pie title 时间分配\n    \"工作\" : 8\n    \"休息\" : 8\n    \"其他\" : 8"},
            {QCoreApplication::translate("MainWindow", "脑图"), "mindmap\n    root((项目计划))\n        需求\n            用户调研\n            功能列表\n        开发\n            编码\n            测试"},
            {QCoreApplication::translate("MainWindow", "坐标轴图"), "xychart\n    title \"月度销售额\"\n    x-axis [\"一月\", \"二月\", \"三月\", \"四月\"]\n    y-axis \"销售额\" 0 --> 100\n    bar [30, 50, 70, 90]\n    line [20, 40, 60, 80]"},
            {QCoreApplication::translate("MainWindow", "数字时序图"), R"({
  "signal": [
    { "name": "clk",  "wave": "p........" },
    { "name": "addr", "wave": "x.====x.", "data": ["A0", "A1"] },
    { "name": "wr",   "wave": "0.1..0.." },
    { "name": "data", "wave": "x..===x.", "data": ["D0"] }
  ]
})",
             "wavedrom"}};
    for (const auto &diagram: diagrams) {
        diagramMenu->addAction(diagram.title, this, [this, diagram] {
            if (!documentNote(currentNoteId())) return;
            setPreview(false);
            auto cursor = editor_->textCursor();
            cursor.beginEditBlock();
            cursor.insertText("\n\n```" + diagram.language + "\n" + diagram.source + "\n```\n\n");
            cursor.endEditBlock();
            editor_->setTextCursor(cursor);
            editor_->setFocus();
        });
    }
    more->setMenu(diagramMenu);
    layout->addWidget(more);
    auto *modes = new QWidget(toolbar_);
    modes->setProperty("alignRight", true);
    modes->setObjectName("viewModes");
    auto *modeLayout = row(modes, 3, 3, 2);
    editMode_ = button({}, "editMode", modes);
    editMode_->setIcon(icon("edit"));
    editMode_->setIconSize(QSize(20, 20));
    editMode_->setToolTip(QCoreApplication::translate("MainWindow", "编辑"));
    previewMode_ = button({}, "previewMode", modes);
    previewMode_->setIcon(icon("preview"));
    previewMode_->setIconSize(QSize(20, 20));
    previewMode_->setToolTip(QCoreApplication::translate("MainWindow", "预览"));
    splitMode_ = button({}, "splitMode", modes);
    splitMode_->setIcon(icon("split"));
    splitMode_->setIconSize(QSize(20, 20));
    splitMode_->setToolTip(QCoreApplication::translate("MainWindow", "分屏"));
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

QString EditorWorkspace::currentNoteId() const {
    return suspended_ ? QString() : tabs_->tabData(tabs_->currentIndex()).toString();
}

Note *EditorWorkspace::documentNote(const QString &id) {
    auto it = externalFiles_.find(id);
    return it == externalFiles_.end() ? store_.note(id) : &it.value();
}

void EditorWorkspace::openExternalFile(const QString &path) {
    const QFileInfo info(path);
    const auto canonicalPath = info.canonicalFilePath();
    if (!info.isFile()) return;
    for (const auto &book: store_.notebooks) {
        if (book.type == "todo") continue;
        for (const auto &note: book.notes) {
            if (!note.folder && QFileInfo(QDir(book.path).filePath(note.path)).canonicalFilePath() == canonicalPath) {
                openNote(note.id);
                return;
            }
        }
    }
    const auto id = QUrl::fromLocalFile(canonicalPath).toString();
    if (!externalFiles_.contains(id)) {
        QFile file(canonicalPath);
        if (!file.open(QIODevice::ReadOnly)) {
            QMessageBox::warning(this, QCoreApplication::translate("MainWindow", "无法读取笔记"), file.errorString());
            return;
        }
        const auto bytes = file.readAll();
        QStringDecoder decoder(QStringDecoder::Utf8, QStringDecoder::Flag::Stateless);
        const QString content = decoder(bytes);
        if (decoder.hasError() || content.contains(QChar::Null)) {
            QMessageBox::warning(this, QCoreApplication::translate("MainWindow", "无法读取笔记"), QCoreApplication::translate("MainWindow", "文件不是有效的 UTF-8 文本：%1").arg(path));
            return;
        }
        Note note{id, {}, info.fileName(), content, false, {}, canonicalPath};
        note.metadata["utf8Bom"] = bytes.startsWith("\xef\xbb\xbf");
        note.metadata["crlf"] = bytes.contains("\r\n");
        note.content.replace("\r\n", "\n");
        externalFiles_.insert(id, note);
    }
    openNote(id);
}

QString EditorWorkspace::noteDirectory(const QString &id) {
    if (externalFiles_.contains(id)) return QFileInfo(externalFiles_[id].path).absolutePath();
    const auto *note = store_.note(id);
    const auto *book = store_.owner(id);
    if (!note || !book) return {};
    return note->path.isEmpty() ? book->path : QFileInfo(book->path + '/' + note->path).absolutePath();
}

void EditorWorkspace::openLocalLink(const QUrl &url) {
    const QFileInfo target(url.toLocalFile());
    if (!target.exists()) {
        QMessageBox::warning(this, QCoreApplication::translate("MainWindow", "无法打开链接"), QCoreApplication::translate("MainWindow", "链接目标不存在：%1").arg(target.filePath()));
        return;
    }
    const auto path = target.canonicalFilePath();
    for (const auto &book: store_.notebooks) {
        if (book.type == "todo") continue;
        for (const auto &note: book.notes) {
            if (note.folder || QFileInfo(book.path + '/' + note.path).canonicalFilePath() != path) continue;
            const auto bookId = book.id;
            const auto noteId = note.id;
            const auto mode = viewMode_;
            emit notebookRequested(bookId);
            openNote(noteId);
            setViewMode(mode);
            preview_->scrollToAnchor(url.fragment(QUrl::FullyDecoded));
            return;
        }
    }
    if (!QDesktopServices::openUrl(url)) {
        QMessageBox::warning(this, QCoreApplication::translate("MainWindow", "无法打开链接"), QCoreApplication::translate("MainWindow", "无法打开链接目标：%1").arg(target.filePath()));
    }
}

void EditorWorkspace::openNote(const QString &id, bool edit) {
    const auto *owner = store_.owner(id);
    if (owner && owner->type == "todo") return;
    for (int index = 0; index < tabs_->count(); ++index) {
        if (tabs_->tabData(index).toString() == id) {
            tabs_->setCurrentIndex(index);
            if (edit) setPreview(false);
            return;
        }
    }
    const auto *note = documentNote(id);
    if (!note || note->folder) return;
    {
        const QSignalBlocker blocker(tabs_);
        const int index = tabs_->addTab(note->title);
        tabs_->setTabData(index, id);
        tabs_->setTabToolTip(index, note->title);
        tabs_->setCurrentIndex(index);
    }
    emit currentDocumentChanged();
    if (edit) setPreview(false);
}

void EditorWorkspace::showTabMenu(const QPoint &position) {
    const int index = tabs_->tabAt(position);
    if (index < 0) return;
    const auto id = tabs_->tabData(index).toString();
    const auto *note = documentNote(id);
    const auto *book = store_.owner(id);
    if (!note) return;
    const auto path = book ? (note->path.isEmpty() ? QString() : QDir(book->path).absoluteFilePath(note->path)) : note->path;
    const auto bookId = book ? book->id : QString();
    QMenu menu(this);
    auto *copyRelativePath = menu.addAction(QCoreApplication::translate("MainWindow", "复制相对路径"), this, [relativePath = note->path] {
        QApplication::clipboard()->setText(QDir::toNativeSeparators(relativePath));
    });
    copyRelativePath->setEnabled(book && !path.isEmpty());
    auto *copyPath = menu.addAction(QCoreApplication::translate("MainWindow", "复制绝对路径"), this, [path] {
        QApplication::clipboard()->setText(QDir::toNativeSeparators(path));
    });
    copyPath->setEnabled(!path.isEmpty());
    auto *openPath = menu.addAction(QCoreApplication::translate("MainWindow", "打开路径"), this, [path] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
    });
    openPath->setEnabled(!path.isEmpty());
    auto *locate = menu.addAction(QCoreApplication::translate("MainWindow", "定位节点"), this, [this, index, id, bookId] {
        emit notebookRequested(bookId);
        tabs_->setCurrentIndex(index);
        emit locateRequested(id);
    });
    locate->setEnabled(book);
    menu.addSeparator();
    menu.addAction(QCoreApplication::translate("MainWindow", "全部保存"), this, [this] { save_(); });
    menu.addAction(QCoreApplication::translate("MainWindow", "关闭标签"), this, [this, index] { closeTab(index); });
    const auto closeTabs = [this, id](bool keepTarget) {
        if (!save_()) return;
        {
            const QSignalBlocker blocker(tabs_);
            for (int tab = tabs_->count() - 1; tab >= 0; --tab) {
                const auto noteId = tabs_->tabData(tab).toString();
                if (keepTarget && noteId == id) continue;
                tabs_->removeTab(tab);
                removeEditor(noteId);
            }
        }
        emit currentDocumentChanged();
    };
    auto *closeOthers = menu.addAction(QCoreApplication::translate("MainWindow", "关闭其他标签"), this, [closeTabs] { closeTabs(true); });
    closeOthers->setEnabled(tabs_->count() > 1);
    menu.addAction(QCoreApplication::translate("MainWindow", "关闭所有标签"), this, [closeTabs] { closeTabs(false); });
    menu.exec(tabs_->mapToGlobal(position));
}

void EditorWorkspace::closeTab(int index) {
    if (!save_()) return;
    const auto id = tabs_->tabData(index).toString();
    {
        const QSignalBlocker blocker(tabs_);
        tabs_->removeTab(index);
    }
    removeEditor(id);
    emit currentDocumentChanged();
}

void EditorWorkspace::activateEditor(const QString &id) {
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
        textEdit->viewport()->installEventFilter(this);
        textEdit->setPlaceholderText(QCoreApplication::translate("MainWindow", "开始书写 Markdown..."));
        textEdit->setProperty("noteId", id);
        session.editor = documentEditor;
        editors_->addWidget(documentEditor);
        editorSessions_.insert(id, session);
        connect(textEdit, &QTextEdit::undoAvailable, this, [this, id](bool available) {
            if (activeEditorId_ == id) undo_->setEnabled(available);
        });
        connect(textEdit, &QTextEdit::redoAvailable, this, [this, id](bool available) {
            if (activeEditorId_ == id) redo_->setEnabled(available);
        });
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
        documentEditor->setBasePath(noteDirectory(id));
        documentEditor->setText(documentNote(id)->content);
        connect(textEdit, &QTextEdit::textChanged, this, [this, id, textEdit] {
            auto *note = documentNote(id);
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
            emit saveRequested();
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
    undo_->setEnabled(editor_->document()->isUndoAvailable());
    redo_->setEnabled(editor_->document()->isRedoAvailable());
    markdownEditor_->setObjectName("markdownEditor");
    editor_->setObjectName("editor");
    editors_->setCurrentWidget(markdownEditor_);
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

void EditorWorkspace::removeEditor(const QString &id) {
    if (!editorSessions_.contains(id)) return;
    auto session = editorSessions_.take(id);
    if (activeEditorId_ == id) {
        activeEditorId_.clear();
        markdownEditor_ = nullptr;
        editor_ = nullptr;
    }
    editors_->removeWidget(session.editor);
    disconnect(session.editor, nullptr, this, nullptr);
    disconnect(session.editor->getTextEdit(), nullptr, this, nullptr);
    disconnect(session.editor->getTextEdit()->verticalScrollBar(), nullptr, this, nullptr);
    delete session.editor;
    externalFiles_.remove(id);
}

void EditorWorkspace::setPreview(bool enabled) {
    setViewMode(enabled ? ViewMode::Preview : ViewMode::Edit);
}

void EditorWorkspace::setViewMode(ViewMode mode) {
    const auto *note = documentNote(currentNoteId());
    if (!note) return;
    const auto oldMode = viewMode_;
    const auto sessionMode = editorSessions_[activeEditorId_].mode;
    const int line = sessionMode == ViewMode::Preview ? editorSessions_[activeEditorId_].previewLine : markdownEditor_->getTopLine();
    const QScopedValueRollback<bool> guard(syncingPreviewScroll_, true);
    viewMode_ = mode;
    markdownEditor_->setSplitMode(mode == ViewMode::Split);
    editMode_->setChecked(mode == ViewMode::Edit);
    previewMode_->setChecked(mode == ViewMode::Preview);
    splitMode_->setChecked(mode == ViewMode::Split);
    editors_->setVisible(mode != ViewMode::Preview);
    editorSessions_[activeEditorId_].mode = mode;
    preview_->setVisible(mode != ViewMode::Edit);
    pages_->setCurrentIndex(1);
    if (mode != ViewMode::Edit) preview_->setMarkdown(note->content, note->id, noteDirectory(note->id));
    else
        previewTimer_->stop();
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

void EditorWorkspace::refreshOutline() {
    if (suspended_) return;
    outline_->clear();
    const auto *note = documentNote(currentNoteId());
    outlineHint_->setText(note ? QCoreApplication::translate("MainWindow", "本文暂无标题") : QCoreApplication::translate("MainWindow", "打开一篇笔记后自动生成大纲"));
    if (note && editorSessions_.contains(note->id)) {
        QList<QPair<int, QTreeWidgetItem *>> parents;
        for (const auto &heading: editorSessions_[note->id].headings) {
            while (!parents.isEmpty() && parents.last().first >= heading.level) parents.removeLast();
            auto *item = new QTreeWidgetItem;
            item->setText(0, heading.title);
            item->setData(0, Qt::UserRole, heading.line);
            if (parents.isEmpty()) outline_->addTopLevelItem(item);
            else
                parents.last().second->addChild(item);
            parents.append({heading.level, item});
        }
        outline_->expandAll();
    }
    outlineHint_->setVisible(outline_->topLevelItemCount() == 0);
}

void EditorWorkspace::updateStatus() {
    const auto *note = documentNote(currentNoteId());
    if (!note) return;
    status_->setText(QCoreApplication::translate("MainWindow", "%1 字 · %2 行 · %3").arg(note->content.size()).arg(note->content.count('\n') + 1).arg(viewMode_ == ViewMode::Split ? QCoreApplication::translate("MainWindow", "分屏") : viewMode_ == ViewMode::Preview ? QCoreApplication::translate("MainWindow", "预览")
                                                                                                                                                                                                                                                                        : QCoreApplication::translate("MainWindow", "编辑中")));
}

bool EditorWorkspace::saveExternalFilesAndSync() {
    for (auto &note: externalFiles_) {
        if (!note.contentDirty) continue;
        auto content = note.content;
        if (note.metadata["crlf"].toBool()) content.replace("\n", "\r\n");
        auto bytes = content.toUtf8();
        if (note.metadata["utf8Bom"].toBool()) bytes.prepend("\xef\xbb\xbf");
        QSaveFile file(note.path);
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
            QMessageBox::warning(this, QCoreApplication::translate("MainWindow", "无法保存笔记"), file.errorString());
            return false;
        }
        note.contentDirty = false;
    }
    for (auto it = editorSessions_.cbegin(); it != editorSessions_.cend(); ++it) {
        const auto directory = noteDirectory(it.key());
        const auto *note = documentNote(it.key());
        if (note && it->editor->getText() != note->content) {
            const auto previous = it->editor->getTextEdit()->textCursor();
            const int anchor = previous.anchor();
            const int position = previous.position();
            it->editor->setText(note->content);
            auto cursor = it->editor->getTextEdit()->textCursor();
            const int end = it->editor->document()->characterCount() - 1;
            cursor.setPosition(qMin(anchor, end));
            cursor.setPosition(qMin(position, end), QTextCursor::KeepAnchor);
            it->editor->getTextEdit()->setTextCursor(cursor);
        }
        if (it->editor->getBasePath() == directory) continue;
        it->editor->setBasePath(directory);
        it->editor->getPreviewMgr()->refreshPreview();
    }
    if (viewMode_ != ViewMode::Edit) {
        if (const auto *note = documentNote(currentNoteId())) preview_->setMarkdown(note->content, note->id, noteDirectory(note->id));
    }
    for (int index = 0; index < tabs_->count(); ++index) {
        if (const auto *note = documentNote(tabs_->tabData(index).toString())) {
            tabs_->setTabText(index, note->title);
            tabs_->setTabToolTip(index, note->title);
            tabs_->setTabIcon(index, {});
        }
    }
    return true;
}

bool EditorWorkspace::eventFilter(QObject *watched, QEvent *event) {
    if (((watched == pages_ && !suspended_) || (editor_ && watched == editor_->viewport())) && (event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove || event->type() == QEvent::Drop)) {
        auto *drop = dynamic_cast<QDropEvent *>(event);
        if (!drop) return QWidget::eventFilter(watched, event);
        QStringList paths;
        for (const auto &url: drop->mimeData()->urls()) {
            if (!url.isLocalFile()) continue;
            const QFileInfo info(url.toLocalFile());
            if (!info.isFile()) continue;
            const auto mime = QMimeDatabase().mimeTypeForFile(info);
            if (info.size() == 0 || (mime.inherits("text/plain") && !mime.name().startsWith("image/"))) paths.append(info.filePath());
        }
        if (!paths.isEmpty()) {
            drop->setDropAction(Qt::CopyAction);
            drop->accept();
            if (event->type() == QEvent::Drop) {
                for (const auto &path: paths) openExternalFile(path);
            }
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void EditorWorkspace::refresh(bool suspended) {
    suspended_ = suspended;
    if (suspended_) {
        previewTimer_->stop();
        return;
    }
    const auto id = currentNoteId();
    const auto *note = documentNote(id);
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
}

void EditorWorkspace::restoreSession(const QStringList &ids, const QString &currentId) {
    for (const auto &id: ids) {
        const QUrl url(id);
        if (url.isLocalFile() && QFileInfo::exists(url.toLocalFile())) openExternalFile(url.toLocalFile());
        else
            openNote(id);
    }
    for (int index = 0; index < tabs_->count(); ++index) {
        if (tabs_->tabData(index).toString() == currentId) {
            tabs_->setCurrentIndex(index);
            break;
        }
    }
}

QStringList EditorWorkspace::openDocuments() const {
    QStringList ids;
    for (int index = 0; index < tabs_->count(); ++index) ids << tabs_->tabData(index).toString();
    return ids;
}

void EditorWorkspace::removeDocuments(const QSet<QString> &ids) {
    const QSignalBlocker blocker(tabs_);
    for (int index = tabs_->count() - 1; index >= 0; --index) {
        const auto id = tabs_->tabData(index).toString();
        if (!ids.contains(id)) continue;
        tabs_->removeTab(index);
        removeEditor(id);
    }
}

void EditorWorkspace::reloadSettings() {
    for (const auto &session: editorSessions_) session.editor->reloadSettings();
}

void EditorWorkspace::showStatus(const QString &text) {
    status_->setText(text);
}
