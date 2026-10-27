#include "EditorWorkspace.h"
#include "FlowLayout.h"
#include "MarkdownEditor.h"
#include "MarkdownPreview.h"
#include "UiStyle.h"
#include <Definition>
#include <QApplication>
#include <QButtonGroup>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QDesktopServices>
#include <QDir>
#include <QDropEvent>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QImageReader>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QMimeDatabase>
#include <QPainter>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScopedPointer>
#include <QScopedValueRollback>
#include <QScrollArea>
#include <QScrollBar>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStackedWidget>
#include <QTabBar>
#include <QTextBlock>
#include <QTimer>
#include <QTreeWidget>
#include <QWheelEvent>
#include <QtConcurrentRun>
#include <Repository>
#include <SyntaxHighlighter>
#include <Theme>
#include <cmath>
#include <vtextedit/previewmgr.h>
#include <vtextedit/texteditutils.h>
#include <vtextedit/vmarkdowneditor.h>
#include <vtextedit/vtextedit.h>

namespace {
QIcon icon(const QString &name) { return QIcon(":/icons/" + name + ".svg"); }
class PlainTextEditor : public QPlainTextEdit {
public:
    explicit PlainTextEditor(QWidget *parent) : QPlainTextEdit(parent), lineNumberArea_(new QWidget(this)) {
        lineNumberArea_->setObjectName("lineNumberArea");
        lineNumberArea_->installEventFilter(this);
        connect(this, &QPlainTextEdit::blockCountChanged, this, [this] { updateLineNumberArea(); });
        connect(this, &QPlainTextEdit::updateRequest, this, [this](const QRect &rect, int dy) {
            if (dy != 0) lineNumberArea_->scroll(0, dy);
            else
                lineNumberArea_->update(0, rect.y(), lineNumberArea_->width(), rect.height());
            if (rect.contains(viewport()->rect())) updateLineNumberArea();
        });
        connect(this, &QPlainTextEdit::cursorPositionChanged, this, &PlainTextEditor::highlightCurrentLine);
        updateLineNumberArea();
        highlightCurrentLine();
    }

protected:
    void resizeEvent(QResizeEvent *event) override {
        QPlainTextEdit::resizeEvent(event);
        updateLineNumberArea();
    }

    void changeEvent(QEvent *event) override {
        QPlainTextEdit::changeEvent(event);
        if (event->type() == QEvent::FontChange) updateLineNumberArea();
    }

    void contextMenuEvent(QContextMenuEvent *event) override {
        const QScopedPointer<QMenu> menu(createStandardContextMenu());
        menu->addSeparator();
        auto *showLineNumbers = menu->addAction(QCoreApplication::translate("PlainTextEditor", "显示行号"));
        showLineNumbers->setCheckable(true);
        showLineNumbers->setChecked(lineNumbersVisible_);
        connect(showLineNumbers, &QAction::toggled, this, [this](bool checked) {
            lineNumbersVisible_ = checked;
            updateLineNumberArea();
        });
        menu->exec(event->globalPos());
    }

    bool eventFilter(QObject *watched, QEvent *event) override {
        if (watched != lineNumberArea_ || event->type() != QEvent::Paint) return QPlainTextEdit::eventFilter(watched, event);
        QPainter painter(lineNumberArea_);
        painter.fillRect(lineNumberArea_->rect(), palette().color(QPalette::AlternateBase));
        painter.setPen(palette().color(QPalette::PlaceholderText));
        painter.setFont(font());
        auto block = firstVisibleBlock();
        qreal top = blockBoundingGeometry(block).translated(contentOffset()).top();
        while (block.isValid() && top < lineNumberArea_->height()) {
            const qreal height = blockBoundingRect(block).height();
            if (block.isVisible() && top + height >= 0) {
                painter.drawText(0, qRound(top), lineNumberArea_->width() - 6, fontMetrics().height(), Qt::AlignRight, QString::number(block.blockNumber() + 1));
            }
            top += height;
            block = block.next();
        }
        return true;
    }

private:
    void highlightCurrentLine() {
        QTextEdit::ExtraSelection selection;
        selection.format.setBackground(QColor(245, 247, 250));
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();
        selection.cursor.clearSelection();
        setExtraSelections({selection});
    }

    void updateLineNumberArea() {
        const int width = lineNumbersVisible_ ? 12 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * static_cast<int>(QString::number(blockCount()).size()) : 0;
        setViewportMargins(width, 0, 0, 0);
        lineNumberArea_->setGeometry(contentsRect().left(), viewport()->geometry().top(), width, viewport()->height());
        lineNumberArea_->setVisible(lineNumbersVisible_);
        lineNumberArea_->update();
    }

    QWidget *lineNumberArea_;
    bool lineNumbersVisible_ = true;
};

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
    preview_->installEventFilter(this);
    connect(preview_, &MarkdownPreview::localLinkActivated, this, &EditorWorkspace::openLocalLink);
    editorSplitter_->addWidget(preview_);
    preview_->hide();
    imageView_ = new QScrollArea(pages_);
    imageView_->setObjectName("imageView");
    imageView_->setAlignment(Qt::AlignCenter);
    image_ = new QWidget(imageView_);
    image_->installEventFilter(this);
    imageView_->setWidget(image_);
    imageView_->viewport()->installEventFilter(this);
    pages_->addWidget(imageView_);
    previewTimer_ = new QTimer(this);
    previewTimer_->setSingleShot(true);
    previewTimer_->setInterval(180);
    connect(previewTimer_, &QTimer::timeout, this, [this] {
        if (const auto *note = documentNote(currentNoteId()); note && note->kind == Note::Kind::Markdown) preview_->setMarkdown(note->content, note->id, noteDirectory(note->id));
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
    auto *filterRow = new QWidget(outlinePanel_);
    auto *filterLayout = row(filterRow, 18, 0, 0);
    outlineFilter_ = new QLineEdit(filterRow);
    outlineFilter_->setObjectName("outlineFilter");
    outlineFilter_->setPlaceholderText(QCoreApplication::translate("MainWindow", "筛选大纲"));
    outlineFilter_->setAccessibleName(QCoreApplication::translate("MainWindow", "筛选大纲"));
    outlineFilter_->setClearButtonEnabled(true);
    outlineFilter_->addAction(icon("search"), QLineEdit::LeadingPosition);
    filterLayout->addWidget(outlineFilter_);
    rightLayout->addWidget(filterRow);
    connect(outlineFilter_, &QLineEdit::textChanged, this, &EditorWorkspace::applyOutlineFilter);
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
    connect(outline_, &QTreeWidget::itemCollapsed, this, [this](QTreeWidgetItem *item) {
        const auto key = item->data(0, Qt::UserRole + 1).toString();
        if (!key.isEmpty()) collapsedSourceSymbols_[currentNoteId()].insert(key);
    });
    connect(outline_, &QTreeWidget::itemExpanded, this, [this](QTreeWidgetItem *item) {
        const auto key = item->data(0, Qt::UserRole + 1).toString();
        if (!key.isEmpty()) collapsedSourceSymbols_[currentNoteId()].remove(key);
    });
    connect(outline_, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem *item) {
        const int line = item->data(0, Qt::UserRole).toInt();
        if (auto *textEditor = textEditors_.value(currentNoteId())) {
            const auto block = textEditor->document()->findBlockByNumber(line);
            if (!block.isValid()) return;
            textEditor->setTextCursor(QTextCursor(block));
            textEditor->setFocus();
            textEditor->centerCursor();
            return;
        }
        if (!editor_) return;
        const QScopedValueRollback<bool> guard(syncingPreviewScroll_, true);
        if (viewMode_ == ViewMode::Preview) {
            preview_->scrollToLine(line, true);
        } else {
            editor_->setTextCursor(QTextCursor(editor_->document()->findBlockByNumber(line)));
            editor_->setFocus();
            vte::TextEditUtils::scrollBlockInPage(editor_, line, vte::TextEditUtils::PagePosition::Center);
            if (viewMode_ == ViewMode::Split) preview_->scrollToLine(line, true);
        }
    });
    outlineTimer_ = new QTimer(this);
    outlineTimer_->setSingleShot(true);
    outlineTimer_->setInterval(250);
    connect(outlineTimer_, &QTimer::timeout, this, &EditorWorkspace::refreshOutline);
    sourceOutlineWatcher_ = new QFutureWatcher<SourceOutline::Result>(this);
    connect(sourceOutlineWatcher_, &QFutureWatcher<SourceOutline::Result>::finished, this, [this] {
        if (suspended_) return;
        const auto *note = documentNote(currentNoteId());
        if (!note || note->kind != Note::Kind::Text) return;
        if (note->id == sourceOutlineId_ && note->path == sourceOutlinePath_ && note->content == sourceOutlineContent_) showSourceOutline(sourceOutlineWatcher_->result());
        else
            outlineTimer_->start();
    });
    auto *find = new QShortcut(QKeySequence::Find, window());
    connect(find, &QShortcut::activated, this, [this] {
        if (const auto *note = documentNote(currentNoteId()); note && note->kind == Note::Kind::Markdown) {
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
        if (!markdownEditor_) return;
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
    for (auto &book: store_.notebooks) {
        if (book.type == "todo") continue;
        const auto relative = QDir(book.path).relativeFilePath(info.absoluteFilePath());
        if (book.type == "normal" && !QDir::isAbsolutePath(relative) && relative != ".." && !relative.startsWith("../")) {
            QString error;
            if (!NotebookStore::loadDirectory(book, QFileInfo(relative).path(), &error)) {
                QMessageBox::warning(this, QCoreApplication::translate("MainWindow", "无法打开文件"), error);
                return;
            }
        }
        for (const auto &note: book.notes) {
            if (!note.folder && QFileInfo(QDir(book.path).filePath(note.path)).canonicalFilePath() == canonicalPath) {
                openNote(note.id);
                return;
            }
        }
    }
    const auto id = QUrl::fromLocalFile(canonicalPath).toString();
    if (!externalFiles_.contains(id)) {
        Note note{id, {}, info.fileName(), {}, false, {}, canonicalPath};
        QString error;
        if (!NotebookStore::readFile(note, canonicalPath, &error)) {
            QMessageBox::warning(this, QCoreApplication::translate("MainWindow", "无法打开文件"), error);
            return;
        }
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
    auto *note = documentNote(id);
    if (!note || note->folder) return;
    const auto path = owner ? QDir(owner->path).filePath(note->path) : note->path;
    if ((owner && owner->type == "normal") || externalFiles_.contains(id)) {
        QString error;
        if (!note->contentDirty && !NotebookStore::readFile(*note, path, &error)) {
            QMessageBox::warning(this, QCoreApplication::translate("MainWindow", "无法打开文件"), error);
            return;
        }
    }
    if (note->kind == Note::Kind::Unsupported || (note->kind == Note::Kind::Image && !QImageReader(path).canRead())) {
        QMessageBox::warning(this, QCoreApplication::translate("MainWindow", "无法打开文件"),
                             QCoreApplication::translate("MainWindow", "无法打开此类型的文件：%1").arg(note->title));
        return;
    }
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

void EditorWorkspace::deactivateEditor() {
    if (editorSessions_.contains(activeEditorId_)) {
        auto &previous = editorSessions_[activeEditorId_];
        previous.verticalScroll = editor_->verticalScrollBar()->value();
        previous.horizontalScroll = editor_->horizontalScrollBar()->value();
        markdownEditor_->setObjectName("inactiveMarkdownEditor");
        editor_->setObjectName("inactiveEditor");
    }
    previewTimer_->stop();
    activeEditorId_.clear();
    markdownEditor_ = nullptr;
    editor_ = nullptr;
}

void EditorWorkspace::activatePlainText(const QString &id) {
    if (!textEditors_.contains(id)) {
        static KSyntaxHighlighting::Repository syntaxRepository;
        if (syntaxRepository.customSearchPaths().isEmpty()) syntaxRepository.addCustomSearchPath(":/demo/data");
        auto *editor = new PlainTextEditor(pages_);
        editor->setObjectName("plainTextEditor");
        const auto definition = syntaxRepository.definitionForFileName(documentNote(id)->path);
        if (definition.isValid()) {
            auto *highlighter = new KSyntaxHighlighting::SyntaxHighlighter(editor->document());
            highlighter->setTheme(syntaxRepository.defaultTheme(editor->palette().color(QPalette::Base).lightness() < 128 ? KSyntaxHighlighting::Repository::DarkTheme : KSyntaxHighlighting::Repository::LightTheme));
            highlighter->setDefinition(definition);
        }
        editor->setPlainText(documentNote(id)->content);
        textEditors_.insert(id, editor);
        pages_->addWidget(editor);
        connect(editor, &QPlainTextEdit::textChanged, this, [this, id, editor] {
            auto *note = documentNote(id);
            if (!note || note->content == editor->toPlainText()) return;
            note->content = editor->toPlainText();
            note->contentDirty = true;
            for (int index = 0; index < tabs_->count(); ++index) {
                if (tabs_->tabData(index).toString() == id) tabs_->setTabIcon(index, icon("dirty"));
            }
            updateStatus();
            if (currentNoteId() == id) outlineTimer_->start();
            emit saveRequested();
        });
    }
    pages_->setCurrentWidget(textEditors_[id]);
    updateStatus();
}

void EditorWorkspace::activateEditor(const QString &id) {
    if (activeEditorId_ == id) return;
    deactivateEditor();
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
    collapsedSourceSymbols_.remove(id);
    if (imageId_ == id) {
        imageId_.clear();
        imagePixmap_ = {};
        imageScale_ = 0;
        updateImage();
    }
    if (auto *editor = textEditors_.take(id)) {
        pages_->removeWidget(editor);
        delete editor;
    }
    externalFiles_.remove(id);
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
    if (!note || note->kind != Note::Kind::Markdown || !markdownEditor_) return;
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
    outline_->setRootIsDecorated(note && note->kind == Note::Kind::Text);
    if (note && note->kind == Note::Kind::Text) {
        const bool matches = note->id == sourceOutlineId_ && note->path == sourceOutlinePath_ && note->content == sourceOutlineContent_;
        if (matches && sourceOutlineWatcher_->isFinished()) {
            showSourceOutline(sourceOutlineWatcher_->result());
            return;
        }
        outlineHint_->setText(tr("正在解析源码..."));
        outlineHint_->show();
        if (sourceOutlineWatcher_->isRunning()) {
            if (!matches) outlineTimer_->start();
            return;
        }
        sourceOutlineId_ = note->id;
        sourceOutlinePath_ = note->path;
        sourceOutlineContent_ = note->content;
        sourceOutlineWatcher_->setFuture(QtConcurrent::run(&SourceOutline::parse, sourceOutlinePath_, sourceOutlineContent_));
        return;
    }
    outlineHint_->setText(note ? QCoreApplication::translate("MainWindow", "本文暂无标题") : QCoreApplication::translate("MainWindow", "打开一篇笔记后自动生成大纲"));
    if (note && note->kind == Note::Kind::Markdown && editorSessions_.contains(note->id)) {
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
    applyOutlineFilter();
}

void EditorWorkspace::applyOutlineFilter() {
    const auto query = outlineFilter_->text().trimmed().toCaseFolded();
    const auto matches = [&query](const QString &text) {
        const auto candidate = text.toCaseFolded();
        int matched = 0;
        for (const auto ch: candidate) {
            if (matched < query.size() && ch == query.at(matched)) ++matched;
        }
        return matched == query.size();
    };
    const auto *note = documentNote(currentNoteId());
    const bool source = note && note->kind == Note::Kind::Text;
    const auto collapsed = collapsedSourceSymbols_.value(currentNoteId());
    const QSignalBlocker blocker(outline_);
    std::function<bool(QTreeWidgetItem *)> filterItem = [&](QTreeWidgetItem *item) {
        bool childVisible = false;
        for (int index = 0; index < item->childCount(); ++index) {
            const bool visible = filterItem(item->child(index));
            childVisible = childVisible || visible;
        }
        const bool visible = matches(item->text(0)) || childVisible;
        item->setHidden(!visible);
        item->setExpanded(query.isEmpty() ? !source || !collapsed.contains(item->data(0, Qt::UserRole + 1).toString()) : childVisible);
        return visible;
    };
    bool anyVisible = false;
    for (int index = 0; index < outline_->topLevelItemCount(); ++index) {
        const bool visible = filterItem(outline_->topLevelItem(index));
        anyVisible = anyVisible || visible;
    }
    if (!query.isEmpty() && outline_->topLevelItemCount() > 0) outlineHint_->setText(QCoreApplication::translate("MainWindow", "没有匹配的大纲"));
    outlineHint_->setVisible(outline_->topLevelItemCount() == 0 || (!query.isEmpty() && !anyVisible));
}

void EditorWorkspace::showSourceOutline(const SourceOutline::Result &result) {
    const QSignalBlocker blocker(outline_);
    outline_->clear();
    QVector<QTreeWidgetItem *> items;
    for (const auto &symbol: result.symbols) {
        auto *item = new QTreeWidgetItem;
        const auto kind = SourceOutline::kindName(symbol.kind);
        item->setText(0, symbol.name);
        switch (symbol.kind) {
            case SourceOutline::Kind::Class:
            case SourceOutline::Kind::Type:
                item->setForeground(0, QColor("#8656b8"));
                break;
            case SourceOutline::Kind::Function:
            case SourceOutline::Kind::Method:
                item->setForeground(0, QColor("#2563b5"));
                break;
            case SourceOutline::Kind::Variable:
            case SourceOutline::Kind::Field:
                item->setForeground(0, QColor("#167863"));
                break;
            case SourceOutline::Kind::Namespace:
                item->setForeground(0, QColor("#63748b"));
                break;
            case SourceOutline::Kind::Enum:
                item->setForeground(0, QColor("#a25f1b"));
                break;
        }
        item->setToolTip(0, symbol.name + '\n' + kind + " · " + tr("第 %1 行").arg(symbol.line + 1));
        item->setData(0, Qt::UserRole, symbol.line);
        QStringList scope{symbol.name};
        for (int parent = symbol.parent; parent >= 0; parent = result.symbols[parent].parent) scope.prepend(result.symbols[parent].name);
        item->setData(0, Qt::UserRole + 1, QString::number(static_cast<int>(symbol.kind)) + ':' + scope.join("::"));
        items.append(item);
    }
    for (int index = 0; index < items.size(); ++index) {
        const int parent = result.symbols[index].parent;
        if (parent < 0) outline_->addTopLevelItem(items[index]);
        else
            items[parent]->addChild(items[index]);
    }
    const auto collapsed = collapsedSourceSymbols_.value(currentNoteId());
    for (auto *item: items) item->setExpanded(!collapsed.contains(item->data(0, Qt::UserRole + 1).toString()));
    outlineHint_->setText(result.supported ? tr("暂无可识别的源码符号") : tr("此文件类型暂不支持源码大纲"));
    applyOutlineFilter();
}

void EditorWorkspace::updateStatus() {
    const auto *note = documentNote(currentNoteId());
    if (!note || note->kind == Note::Kind::Image) return;
    status_->setText(QCoreApplication::translate("MainWindow", "%1 字 · %2 行 · %3").arg(note->content.size()).arg(note->content.count('\n') + 1).arg(note->kind == Note::Kind::Text ? QCoreApplication::translate("MainWindow", "编辑中") : viewMode_ == ViewMode::Split ? QCoreApplication::translate("MainWindow", "分屏")
                                                                                                                                                                                                                                     : viewMode_ == ViewMode::Preview     ? QCoreApplication::translate("MainWindow", "预览")
                                                                                                                                                                                                                                                                          : QCoreApplication::translate("MainWindow", "编辑中")));
}

bool EditorWorkspace::saveExternalFilesAndSync() {
    for (auto &note: externalFiles_) {
        if (!note.contentDirty) continue;
        const auto bytes = NotebookStore::fileContents(note);
        QSaveFile file(note.path);
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
            QMessageBox::warning(this, QCoreApplication::translate("MainWindow", "无法保存笔记"), file.errorString());
            return false;
        }
        note.contentDirty = false;
    }
    for (auto it = textEditors_.cbegin(); it != textEditors_.cend(); ++it) {
        const auto *note = documentNote(it.key());
        if (!note || it.value()->toPlainText() == note->content) continue;
        const QSignalBlocker blocker(it.value());
        const auto cursor = it.value()->textCursor();
        it.value()->setPlainText(note->content);
        auto restored = it.value()->textCursor();
        const int end = it.value()->document()->characterCount() - 1;
        restored.setPosition(qMin(cursor.anchor(), end));
        restored.setPosition(qMin(cursor.position(), end), QTextCursor::KeepAnchor);
        it.value()->setTextCursor(restored);
        if (it.key() == currentNoteId()) outlineTimer_->start();
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
        if (const auto *note = documentNote(currentNoteId()); note && note->kind == Note::Kind::Markdown) preview_->setMarkdown(note->content, note->id, noteDirectory(note->id));
    }
    for (int index = 0; index < tabs_->count(); ++index) {
        if (const auto *note = documentNote(tabs_->tabData(index).toString())) {
            tabs_->setTabText(index, note->title);
            tabs_->setTabToolTip(index, note->title);
            tabs_->setTabIcon(index, note->contentDirty ? icon("dirty") : QIcon());
        }
    }
    return true;
}

void EditorWorkspace::updateImage() {
    if (imagePixmap_.isNull()) {
        image_->resize(0, 0);
        image_->update();
        return;
    }
    const auto available = imageView_->maximumViewportSize();
    const auto scale = imageScale_ > 0 ? imageScale_ : qMin(1.0, qMin(qreal(available.width()) / imagePixmap_.width(), qreal(available.height()) / imagePixmap_.height()));
    image_->resize(qMax(1, qRound(imagePixmap_.width() * scale)), qMax(1, qRound(imagePixmap_.height() * scale)));
    image_->update();
}

bool EditorWorkspace::eventFilter(QObject *watched, QEvent *event) {
    if (watched == image_ && event->type() == QEvent::Paint) {
        QPainter painter(image_);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        painter.drawPixmap(image_->rect(), imagePixmap_);
        return true;
    }
    if (imageView_ && watched == imageView_->viewport() && event->type() == QEvent::Resize) updateImage();
    if (imageView_ && (watched == image_ || watched == imageView_->viewport()) && event->type() == QEvent::Wheel && !imagePixmap_.isNull()) {
        auto *wheel = dynamic_cast<QWheelEvent *>(event);
        if (wheel && wheel->modifiers().testFlag(Qt::ControlModifier)) {
            const int delta = wheel->angleDelta().y() != 0 ? wheel->angleDelta().y() : wheel->pixelDelta().y();
            if (delta == 0) {
                wheel->accept();
                return true;
            }
            const auto point = image_->mapFromGlobal(wheel->globalPosition().toPoint());
            const auto anchor = imageView_->viewport()->mapFromGlobal(wheel->globalPosition().toPoint());
            const auto previousScale = qreal(image_->width()) / imagePixmap_.width();
            imageScale_ = qBound(0.01, previousScale * std::pow(1.2, delta / 120.0), 8.0);
            updateImage();
            const auto ratio = imageScale_ / previousScale;
            imageView_->horizontalScrollBar()->setValue(qRound(point.x() * ratio) - anchor.x());
            imageView_->verticalScrollBar()->setValue(qRound(point.y() * ratio) - anchor.y());
            wheel->accept();
            return true;
        }
    }
    if ((((watched == pages_ || watched == preview_) && !suspended_) || (editor_ && watched == editor_->viewport())) && (event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove || event->type() == QEvent::Drop)) {
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
        outlineTimer_->stop();
        return;
    }
    const auto id = currentNoteId();
    const auto *note = documentNote(id);
    toolbar_->setVisible(note && note->kind == Note::Kind::Markdown);
    if (note && note->kind == Note::Kind::Markdown) {
        activateEditor(id);
        setViewMode(viewMode_);
    } else {
        deactivateEditor();
        if (note && note->kind == Note::Kind::Text) {
            activatePlainText(id);
        } else if (note && note->kind == Note::Kind::Image) {
            const auto *book = store_.owner(id);
            const auto path = book ? QDir(book->path).filePath(note->path) : note->path;
            QImageReader reader(path);
            reader.setAutoTransform(true);
            imagePixmap_ = QPixmap::fromImage(reader.read());
            imagePixmap_.setDevicePixelRatio(1);
            if (imageId_ != id) imageScale_ = 0;
            imageId_ = id;
            pages_->setCurrentWidget(imageView_);
            updateImage();
            status_->setText(imagePixmap_.isNull() ? reader.errorString() : QStringLiteral("%1 × %2").arg(imagePixmap_.width()).arg(imagePixmap_.height()));
        } else {
            pages_->setCurrentIndex(0);
            status_->clear();
        }
        if (preview_->revision() > 0) preview_->setMarkdown({}, {});
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
    for (int index = 0; index < tabs_->count(); ++index) {
        const auto id = tabs_->tabData(index).toString();
        const auto *book = store_.owner(id);
        const auto *note = store_.note(id);
        ids << (book && book->type == "normal" && note ? QUrl::fromLocalFile(QDir(book->path).filePath(note->path)).toString() : id);
    }
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
