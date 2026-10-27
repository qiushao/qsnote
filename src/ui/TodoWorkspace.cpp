#include "TodoWorkspace.h"
#include "FlowLayout.h"
#include "UiStyle.h"
#include "core/NotebookCategories.h"
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPlainTextEdit>
#include <QSignalBlocker>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QTreeWidget>
#include <algorithm>

namespace {
class TodoDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        const auto due = index.data(Qt::UserRole + 1).toString();
        if (due.isEmpty()) {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }
        auto dateOption = option;
        initStyleOption(&dateOption, index);
        dateOption.features &= ~QStyleOptionViewItem::HasCheckIndicator;
        dateOption.text = due;
        dateOption.displayAlignment = Qt::AlignRight | Qt::AlignVCenter;
        auto *style = option.widget->style();
        const int dateWidth = qMin(option.rect.width(), style->sizeFromContents(QStyle::CT_ItemViewItem, &dateOption, {}, option.widget).width());
        auto titleOption = option;
        titleOption.rect.adjust(0, 0, -dateWidth, 0);
        QStyledItemDelegate::paint(painter, titleOption, index);
        dateOption.rect.setLeft(titleOption.rect.right() + 1);
        style->drawControl(QStyle::CE_ItemViewItem, &dateOption, painter, option.widget);
    }
};

}// namespace

TodoWorkspace::TodoWorkspace(NotebookStore &store, QWidget *parent) : QWidget(parent), store_(store) {
    buildUi();
}

void TodoWorkspace::setNotebook(const QString &id, bool storageReady) {
    if (notebookId_ != id) todoPage_ = 0;
    notebookId_ = id;
    storageReady_ = storageReady;
}

void TodoWorkspace::setSearch(const QString &query) {
    if (query_ != query) todoPage_ = 0;
    query_ = query;
}

void TodoWorkspace::refresh() { refreshTodoCategories(); }

void TodoWorkspace::buildUi() {
    categoryPanel_ = new QWidget(this);
    auto *categoryLayout = column(categoryPanel_, 0, 8);
    todoStatus_ = new QComboBox(categoryPanel_);
    todoStatus_->setObjectName("todoStatus");
    todoStatus_->setAccessibleName(QCoreApplication::translate("MainWindow", "事项状态筛选"));
    todoStatus_->addItems({QCoreApplication::translate("MainWindow", "进行中"), QCoreApplication::translate("MainWindow", "已完成")});
    categoryLayout->addWidget(todoStatus_);
    tree_ = new QTreeWidget(categoryPanel_);
    tree_->setObjectName("noteTree");
    tree_->setHeaderHidden(true);
    tree_->setIndentation(14);
    tree_->setTextElideMode(Qt::ElideRight);
    tree_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    tree_->setContextMenuPolicy(Qt::CustomContextMenu);
    tree_->setMinimumSize(0, 0);
    categoryLayout->addWidget(tree_, 1);
    connect(todoStatus_, &QComboBox::currentIndexChanged, this, [this] {
        todoPage_ = 0;
        refreshTodoList();
    });
    connect(tree_, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem *item) {
        if (!item) return;
        todoCategoryFilter_ = item->data(0, Qt::UserRole).toString();
        todoPage_ = 0;
        refreshTodoList();
    });
    connect(tree_, &QTreeWidget::customContextMenuRequested, this, &TodoWorkspace::showCategoryMenu);
    setObjectName("todoListPage");
    auto *listLayout = column(this, 16, 12);
    auto *actions = new QWidget(this);
    actions->setObjectName("todoActions");
    auto *actionsLayout = new FlowLayout(actions, 0, 8);
    listLayout->addWidget(actions);
    auto *addCategory = button(QCoreApplication::translate("MainWindow", "新建分类"), "newTodoCategory", this);
    actionsLayout->addWidget(addCategory);
    connect(addCategory, &QPushButton::clicked, this, &TodoWorkspace::createTodoCategory);
    todoAdd_ = button(QCoreApplication::translate("MainWindow", "新建事项"), "newTodo", this);
    todoAdd_->setIcon(QIcon(":/icons/plus-white.svg"));
    todoAdd_->setIconSize(QSize(13, 13));
    setPrimaryButton(todoAdd_);
    actionsLayout->addWidget(todoAdd_);
    todoClearCompleted_ = button(QCoreApplication::translate("MainWindow", "清空已完成事项"), "clearCompletedTodos", this);
    actionsLayout->addWidget(todoClearCompleted_);
    todoSort_ = new QComboBox(this);
    todoSort_->setObjectName("todoSort");
    todoSort_->setAccessibleName(QCoreApplication::translate("MainWindow", "事项排序"));
    todoSort_->addItems({QCoreApplication::translate("MainWindow", "按优先级（高到低）"), QCoreApplication::translate("MainWindow", "按创建日期（新到旧）"), QCoreApplication::translate("MainWindow", "按截止日期（近到远）")});
    todoSort_->setProperty("alignRight", true);
    actionsLayout->addWidget(todoSort_);
    connect(todoSort_, &QComboBox::currentIndexChanged, this, [this] {
        todoPage_ = 0;
        refreshTodoList();
    });
    todoHint_ = label(QCoreApplication::translate("MainWindow", "暂无进行中的事项"), this, true);
    listLayout->addWidget(todoHint_);
    todoList_ = new QTreeWidget(this);
    todoList_->setObjectName("todoList");
    todoList_->setHeaderHidden(true);
    todoList_->setRootIsDecorated(false);
    todoList_->setMinimumSize(0, 0);
    todoList_->setColumnCount(2);
    todoList_->setItemDelegateForColumn(0, new TodoDelegate(todoList_));
    todoList_->header()->setStretchLastSection(false);
    todoList_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    todoList_->header()->setSectionResizeMode(1, QHeaderView::Fixed);
    todoList_->header()->resizeSection(1, 40);
    listLayout->addWidget(todoList_, 1);
    todoPagination_ = new QWidget(this);
    auto *paginationLayout = row(todoPagination_, 0, 0);
    todoPreviousPage_ = button(QCoreApplication::translate("MainWindow", "上一页"), "todoPreviousPage", todoPagination_);
    todoNextPage_ = button(QCoreApplication::translate("MainWindow", "下一页"), "todoNextPage", todoPagination_);
    todoPageInfo_ = label({}, todoPagination_, true);
    todoPageInfo_->setObjectName("todoPageInfo");
    todoPageInfo_->setAlignment(Qt::AlignCenter);
    paginationLayout->addWidget(todoPreviousPage_);
    paginationLayout->addWidget(todoPageInfo_, 1);
    paginationLayout->addWidget(todoNextPage_);
    listLayout->addWidget(todoPagination_);
    connect(todoPreviousPage_, &QPushButton::clicked, this, [this] {
        --todoPage_;
        refreshTodoList();
    });
    connect(todoNextPage_, &QPushButton::clicked, this, [this] {
        ++todoPage_;
        refreshTodoList();
    });

    todoDetails_ = new QWidget(this);
    todoDetails_->setObjectName("todoDetails");
    auto *detailsLayout = column(todoDetails_, 16, 12);
    auto *detailsTitle = label(QCoreApplication::translate("MainWindow", "事项详情"), todoDetails_, true);
    detailsTitle->setObjectName("sectionTitle");
    detailsLayout->addWidget(detailsTitle);
    todoTitle_ = new QLineEdit(todoDetails_);
    todoTitle_->setObjectName("todoTitle");
    todoTitle_->setPlaceholderText(QCoreApplication::translate("MainWindow", "事项标题"));
    todoTitle_->setAccessibleName(QCoreApplication::translate("MainWindow", "事项标题"));
    detailsLayout->addWidget(todoTitle_);
    auto *createdRow = new QWidget(todoDetails_);
    auto *createdLayout = row(createdRow, 0, 0);
    createdLayout->addWidget(label(QCoreApplication::translate("MainWindow", "创建时间"), createdRow, true));
    todoCreated_ = label({}, createdRow, true);
    todoCreated_->setObjectName("todoCreated");
    todoCreated_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    createdLayout->addWidget(todoCreated_);
    todoDueEnabled_ = new QCheckBox(QCoreApplication::translate("MainWindow", "截止日期"), createdRow);
    todoDueEnabled_->setObjectName("todoDueEnabled");
    todoDue_ = new QDateEdit(QDate::currentDate(), createdRow);
    todoDue_->setObjectName("todoDue");
    todoDue_->setDisplayFormat("yyyy-MM-dd");
    todoDue_->setCalendarPopup(true);
    todoDue_->setAccessibleName(QCoreApplication::translate("MainWindow", "截止日期"));
    todoDue_->setEnabled(false);
    createdLayout->addWidget(todoDueEnabled_);
    createdLayout->addWidget(todoDue_);
    createdLayout->addStretch();
    detailsLayout->addWidget(createdRow);
    auto *propertiesRow = new QWidget(todoDetails_);
    auto *propertiesLayout = row(propertiesRow, 0, 0);
    todoCompleted_ = new QCheckBox(QCoreApplication::translate("MainWindow", "已完成"), propertiesRow);
    todoCompleted_->setObjectName("todoCompleted");
    propertiesLayout->addWidget(todoCompleted_);
    todoCategory_ = new QComboBox(propertiesRow);
    todoCategory_->setObjectName("todoCategory");
    todoCategory_->setAccessibleName(QCoreApplication::translate("MainWindow", "事项分类"));
    propertiesLayout->addWidget(label(QCoreApplication::translate("MainWindow", "分类"), propertiesRow, true));
    propertiesLayout->addWidget(todoCategory_);
    todoPriority_ = new QComboBox(propertiesRow);
    todoPriority_->setObjectName("todoPriority");
    todoPriority_->setAccessibleName(QCoreApplication::translate("MainWindow", "优先级"));
    todoPriority_->addItems({QCoreApplication::translate("MainWindow", "低"), QCoreApplication::translate("MainWindow", "中"), QCoreApplication::translate("MainWindow", "高")});
    propertiesLayout->addWidget(label(QCoreApplication::translate("MainWindow", "优先级"), propertiesRow, true));
    propertiesLayout->addWidget(todoPriority_);
    propertiesLayout->addStretch();
    detailsLayout->addWidget(propertiesRow);
    todoContent_ = new QPlainTextEdit(todoDetails_);
    todoContent_->setObjectName("todoContent");
    todoContent_->setPlaceholderText(QCoreApplication::translate("MainWindow", "选择事项后编辑详情"));
    todoContent_->setAccessibleName(QCoreApplication::translate("MainWindow", "事项详情正文"));
    todoContent_->setMinimumSize(0, 0);
    detailsLayout->addWidget(todoContent_, 1);
    todoDetails_->hide();

    connect(todoAdd_, &QPushButton::clicked, this, &TodoWorkspace::createTodo);
    connect(todoClearCompleted_, &QPushButton::clicked, this, &TodoWorkspace::clearCompletedTodos);
    connect(todoList_, &QTreeWidget::currentItemChanged, this, [this] { refreshTodoDetails(); });
    connect(todoTitle_, &QLineEdit::textEdited, this, [this](const QString &text) {
        auto *item = todoList_->currentItem();
        auto *note = item ? store_.note(item->data(0, Qt::UserRole).toString()) : nullptr;
        if (!note) return;
        note->title = text.trimmed().isEmpty() ? QCoreApplication::translate("MainWindow", "未命名事项") : text.trimmed();
        item->setText(0, note->title);
        item->setToolTip(0, note->title);
        emit saveRequested(true);
    });
    connect(todoContent_, &QPlainTextEdit::textChanged, this, [this] {
        const auto *item = todoList_->currentItem();
        auto *note = item ? store_.note(item->data(0, Qt::UserRole).toString()) : nullptr;
        if (!note) return;
        note->content = todoContent_->toPlainText();
        note->contentDirty = true;
        emit saveRequested(true);
    });
    connect(todoCategory_, &QComboBox::currentIndexChanged, this, [this] {
        const auto *item = todoList_->currentItem();
        auto *note = item ? store_.note(item->data(0, Qt::UserRole).toString()) : nullptr;
        if (!note) return;
        note->metadata["category"] = todoCategory_->currentData().toString();
        save();
        refreshTodoList();
    });
    connect(todoPriority_, &QComboBox::currentIndexChanged, this, [this](int priority) {
        const auto *item = todoList_->currentItem();
        auto *note = item ? store_.note(item->data(0, Qt::UserRole).toString()) : nullptr;
        if (!note) return;
        note->metadata["priority"] = priority;
        const auto id = note->id;
        save();
        refreshTodoList(id);
    });
    connect(todoCompleted_, &QCheckBox::toggled, this, [this](bool checked) {
        const auto *item = todoList_->currentItem();
        auto *note = item ? store_.note(item->data(0, Qt::UserRole).toString()) : nullptr;
        if (!note) return;
        note->metadata["completed"] = checked;
        save();
        refreshTodoList();
    });
    connect(todoDueEnabled_, &QCheckBox::toggled, this, [this](bool checked) {
        auto *item = todoList_->currentItem();
        auto *note = item ? store_.note(item->data(0, Qt::UserRole).toString()) : nullptr;
        if (!note) return;
        todoDue_->setEnabled(checked);
        if (checked) note->metadata["due"] = todoDue_->date().toString(Qt::ISODate);
        else
            note->metadata.remove("due");
        const auto id = note->id;
        save();
        refreshTodoList(id);
    });
    connect(todoDue_, &QDateEdit::dateChanged, this, [this](const QDate &date) {
        if (!todoDueEnabled_->isChecked()) return;
        auto *item = todoList_->currentItem();
        auto *note = item ? store_.note(item->data(0, Qt::UserRole).toString()) : nullptr;
        if (!note) return;
        note->metadata["due"] = date.toString(Qt::ISODate);
        const auto id = note->id;
        save();
        refreshTodoList(id);
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

void TodoWorkspace::refreshTodoCategories(const QString &selectedId) {
    const auto *book = store_.notebook(notebookId_);
    if (!book || book->type != "todo") return;
    if (todoNotebookId_ != book->id) {
        todoNotebookId_ = book->id;
        todoPage_ = 0;
        todoCategoryFilter_.clear();
        const QSignalBlocker blocker(todoStatus_);
        todoStatus_->setCurrentIndex(0);
    }
    auto categories = NotebookCategories::todo(*book);
    const QSignalBlocker treeBlocker(tree_);
    const QSignalBlocker categoryBlocker(todoCategory_);
    tree_->clear();
    tree_->setDragDropMode(QAbstractItemView::NoDragDrop);
    todoCategory_->clear();
    for (const auto &category: categories) {
        todoCategory_->addItem(category == "默认" ? QCoreApplication::translate("MainWindow", "默认") : category, category);
    }
    categories.prepend(QStringLiteral("全部"));
    for (const auto &category: categories) {
        const auto title = category == "全部" ? QCoreApplication::translate("MainWindow", "全部") : category == "默认" ? QCoreApplication::translate("MainWindow", "默认")
                                                                                                                       : category;
        auto *item = new QTreeWidgetItem(tree_, {title});
        const auto filter = category == "全部" ? QString() : category;
        item->setData(0, Qt::UserRole, filter);
        if (filter == todoCategoryFilter_) tree_->setCurrentItem(item);
    }
    refreshTodoList(selectedId);
}

void TodoWorkspace::refreshTodoList(const QString &selectedId) {
    const auto *book = store_.notebook(notebookId_);
    if (!book || book->type != "todo") return;
    const bool completed = todoStatus_->currentIndex() == 1;
    todoAdd_->setVisible(!completed);
    todoClearCompleted_->setVisible(completed);
    const auto *current = todoList_->currentItem();
    const auto id = !selectedId.isEmpty() ? selectedId : current ? current->data(0, Qt::UserRole).toString()
                                                                 : QString();
    const QSignalBlocker blocker(todoList_);
    todoList_->clear();
    const auto query = query_.trimmed();
    constexpr int pageSize = 20;
    QList<const Note *> matches;
    int completedCount = 0;
    for (const auto &note: book->notes) {
        if (!note.folder && note.metadata["completed"].toBool()) ++completedCount;
        if (note.folder || note.metadata["completed"].toBool() != completed) continue;
        if (!todoCategoryFilter_.isEmpty() && note.metadata["category"].toString("默认") != todoCategoryFilter_) continue;
        if (!query.isEmpty() && !note.title.contains(query, Qt::CaseInsensitive) && !note.content.contains(query, Qt::CaseInsensitive)) continue;
        matches.append(&note);
    }
    std::stable_sort(matches.begin(), matches.end(), [this](const Note *left, const Note *right) {
        if (todoSort_->currentIndex() == 0) {
            return left->metadata["priority"].toInt(1) > right->metadata["priority"].toInt(1);
        }
        if (todoSort_->currentIndex() == 1) {
            const auto a = QDateTime::fromString(left->metadata["created"].toString(), Qt::ISODate);
            const auto b = QDateTime::fromString(right->metadata["created"].toString(), Qt::ISODate);
            if (a.isValid() != b.isValid()) return a.isValid();
            return a > b;
        }
        const auto a = QDate::fromString(left->metadata["due"].toString(), Qt::ISODate);
        const auto b = QDate::fromString(right->metadata["due"].toString(), Qt::ISODate);
        if (a.isValid() != b.isValid()) return a.isValid();
        return a < b;
    });
    for (int index = 0; index < matches.size(); ++index) {
        if (matches.at(index)->id == selectedId) todoPage_ = index / pageSize;
    }
    const int count = static_cast<int>(matches.size());
    todoClearCompleted_->setEnabled(completedCount > 0);
    const int pageCount = qMax(1, (count + pageSize - 1) / pageSize);
    todoPage_ = qBound(0, todoPage_, pageCount - 1);
    todoPagination_->setVisible(pageCount > 1);
    todoPreviousPage_->setEnabled(todoPage_ > 0);
    todoNextPage_->setEnabled(todoPage_ + 1 < pageCount);
    todoPageInfo_->setText(QCoreApplication::translate("MainWindow", "第 %1 / %2 页，共 %3 项").arg(todoPage_ + 1).arg(pageCount).arg(count));
    for (int index = todoPage_ * pageSize; index < qMin(count, (todoPage_ + 1) * pageSize); ++index) {
        const auto &note = *matches.at(index);
        auto *item = new QTreeWidgetItem(todoList_, {note.title});
        item->setData(0, Qt::UserRole, note.id);
        item->setData(0, Qt::UserRole + 1, note.metadata["due"].toString());
        item->setToolTip(0, note.title);
        item->setCheckState(0, note.metadata["completed"].toBool() ? Qt::Checked : Qt::Unchecked);
        auto *remove = button(QString(), "todoDelete", todoList_);
        remove->setProperty("iconButton", true);
        remove->setIcon(QIcon(":/icons/close.svg"));
        remove->setIconSize(QSize(12, 12));
        remove->setFocusPolicy(Qt::NoFocus);
        remove->setToolTip(QCoreApplication::translate("MainWindow", "删除"));
        remove->setAccessibleName(QCoreApplication::translate("MainWindow", "删除"));
        connect(remove, &QPushButton::clicked, this, [this, item, noteId = note.id] {
            todoList_->setCurrentItem(item);
            deleteTodo(noteId);
        });
        todoList_->setItemWidget(item, 1, remove);
        if (note.id == id) todoList_->setCurrentItem(item);
    }
    if (!todoList_->currentItem() && todoList_->topLevelItemCount()) todoList_->setCurrentItem(todoList_->topLevelItem(0));
    todoHint_->setText(!query.isEmpty() ? QCoreApplication::translate("MainWindow", "没有匹配的事项") : todoStatus_->currentIndex() == 0 ? QCoreApplication::translate("MainWindow", "暂无进行中的事项")
                                                                                                                                         : QCoreApplication::translate("MainWindow", "暂无已完成的事项"));
    todoHint_->setVisible(todoList_->topLevelItemCount() == 0);
    refreshTodoDetails();
}

void TodoWorkspace::refreshTodoDetails() {
    const auto *item = todoList_->currentItem();
    const auto *note = item ? store_.note(item->data(0, Qt::UserRole).toString()) : nullptr;
    const QSignalBlocker titleBlocker(todoTitle_);
    const QSignalBlocker categoryBlocker(todoCategory_);
    const QSignalBlocker priorityBlocker(todoPriority_);
    const QSignalBlocker completedBlocker(todoCompleted_);
    const QSignalBlocker dueEnabledBlocker(todoDueEnabled_);
    const QSignalBlocker dueBlocker(todoDue_);
    const QSignalBlocker contentBlocker(todoContent_);
    todoDetails_->setEnabled(note);
    todoTitle_->setText(note ? note->title : QString());
    const auto created = QDateTime::fromString(note ? note->metadata["created"].toString() : QString(), Qt::ISODate);
    todoCreated_->setText(created.isValid() ? created.toString("yyyy-MM-dd HH:mm") : QStringLiteral("—"));
    todoCategory_->setCurrentIndex(todoCategory_->findData(note ? note->metadata["category"].toString("默认") : QStringLiteral("默认")));
    todoPriority_->setCurrentIndex(note ? note->metadata["priority"].toInt(1) : 1);
    todoCompleted_->setChecked(note && note->metadata["completed"].toBool());
    const auto due = QDate::fromString(note ? note->metadata["due"].toString() : QString(), Qt::ISODate);
    todoDueEnabled_->setChecked(due.isValid());
    todoDue_->setEnabled(due.isValid());
    todoDue_->setDate(due.isValid() ? due : QDate::currentDate());
    todoContent_->setPlainText(note ? note->content : QString());
}

void TodoWorkspace::createTodo() {
    auto *book = store_.notebook(notebookId_);
    if (!book || book->type != "todo") return;
    QDialog dialog(this);
    dialog.setWindowTitle(QCoreApplication::translate("MainWindow", "新建事项"));
    auto *layout = new QFormLayout(&dialog);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(12);
    layout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    layout->setRowWrapPolicy(QFormLayout::WrapLongRows);
    auto *titleEdit = new QLineEdit(&dialog);
    titleEdit->setObjectName("newTodoTitle");
    layout->addRow(QCoreApplication::translate("MainWindow", "事项标题："), titleEdit);
    auto *category = new QComboBox(&dialog);
    category->setObjectName("newTodoCategory");
    for (int index = 0; index < todoCategory_->count(); ++index) {
        category->addItem(todoCategory_->itemText(index), todoCategory_->itemData(index));
    }
    category->setCurrentIndex(category->findData(todoCategoryFilter_.isEmpty() ? QStringLiteral("默认") : todoCategoryFilter_));
    layout->addRow(QCoreApplication::translate("MainWindow", "分类"), category);
    auto *priority = new QComboBox(&dialog);
    priority->setObjectName("newTodoPriority");
    priority->setAccessibleName(QCoreApplication::translate("MainWindow", "优先级"));
    priority->addItems({QCoreApplication::translate("MainWindow", "低"), QCoreApplication::translate("MainWindow", "中"), QCoreApplication::translate("MainWindow", "高")});
    priority->setCurrentIndex(1);
    layout->addRow(QCoreApplication::translate("MainWindow", "优先级"), priority);
    auto *content = new QPlainTextEdit(&dialog);
    content->setObjectName("todoContent");
    content->setAccessibleName(QCoreApplication::translate("MainWindow", "事项详情正文"));
    content->setMinimumHeight(120);
    layout->addRow(QCoreApplication::translate("MainWindow", "事项详情"), content);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    setPrimaryButton(buttons->button(QDialogButtonBox::Ok));
    layout->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    dialog.resize(560, dialog.sizeHint().height());
    if (dialog.exec() != QDialog::Accepted) return;
    const auto title = titleEdit->text().trimmed();
    if (title.isEmpty()) return;
    Note note;
    note.id = NotebookStore::newId();
    note.title = title;
    note.content = content->toPlainText();
    note.metadata = {{"category", category->currentData().toString()},
                     {"priority", priority->currentIndex()},
                     {"completed", false},
                     {"created", QDateTime::currentDateTime().toString(Qt::ISODate)}};
    book->notes.append(note);
    save();
    {
        const QSignalBlocker statusBlocker(todoStatus_);
        query_.clear();
        emit searchCleared();
        todoStatus_->setCurrentIndex(0);
    }
    if (!todoCategoryFilter_.isEmpty()) todoCategoryFilter_ = category->currentData().toString();
    refreshTodoCategories(note.id);
    todoTitle_->setFocus();
}

void TodoWorkspace::deleteTodo(const QString &noteId) {
    auto *book = store_.notebook(notebookId_);
    if (!book || book->type != "todo") return;
    auto *note = store_.note(noteId);
    if (!note) return;
    const auto hint = QCoreApplication::translate("MainWindow", "确认删除事项「%1」？该操作不可恢复。").arg(note->title);
    if (QMessageBox::question(this, QCoreApplication::translate("MainWindow", "删除事项"), hint, QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes) return;
    if (!storageReady_) {
        save();
        return;
    }
    applyRemoval(store_.removeNotes(book->id, {noteId}));
    refreshTodoList();
}

void TodoWorkspace::clearCompletedTodos() {
    auto *book = store_.notebook(notebookId_);
    if (!book || book->type != "todo") return;
    int count = 0;
    for (const auto &note: book->notes) {
        if (!note.folder && note.metadata["completed"].toBool()) ++count;
    }
    if (count == 0) return;
    const auto hint = QCoreApplication::translate("MainWindow", "确认清空当前笔记本中全部 %1 个已完成事项？包含所有分类和分页中的事项，不受搜索条件限制。该操作不可恢复。").arg(count);
    if (QMessageBox::question(this, QCoreApplication::translate("MainWindow", "清空已完成事项"), hint, QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes) return;
    if (!storageReady_) {
        save();
        return;
    }
    applyRemoval(store_.removeCompletedTodos(book->id));
    todoPage_ = 0;
    refreshTodoCategories();
}

void TodoWorkspace::createTodoCategory() {
    auto *book = store_.notebook(notebookId_);
    if (!book || book->type != "todo") return;
    bool accepted = false;
    const auto name = QInputDialog::getText(this, QCoreApplication::translate("MainWindow", "新建分类"), QCoreApplication::translate("MainWindow", "分类名称："), QLineEdit::Normal, {}, &accepted).trimmed();
    if (!accepted || name.isEmpty()) return;
    QString error;
    if (!NotebookCategories::create(*book, name, &error)) {
        QMessageBox::information(this, QCoreApplication::translate("MainWindow", "无法新建分类"), error);
        return;
    }
    todoCategoryFilter_ = name;
    todoPage_ = 0;
    save();
    refreshTodoCategories();
}

void TodoWorkspace::renameTodoCategory(const QString &name) {
    auto *book = store_.notebook(notebookId_);
    if (!book || book->type != "todo" || name.isEmpty() || name == QStringLiteral("默认")) return;
    bool accepted = false;
    const auto newName = QInputDialog::getText(this, QCoreApplication::translate("MainWindow", "重命名分类"), QCoreApplication::translate("MainWindow", "新的分类名称："), QLineEdit::Normal, name, &accepted).trimmed();
    if (!accepted || newName.isEmpty() || newName == name) return;
    QString error;
    if (!NotebookCategories::rename(*book, name, newName, &error)) {
        QMessageBox::information(this, QCoreApplication::translate("MainWindow", "无法重命名分类"), error);
        return;
    }
    if (todoCategoryFilter_ == name) todoCategoryFilter_ = newName;
    save();
    refreshTodoCategories();
}

void TodoWorkspace::deleteTodoCategory(const QString &name) {
    auto *book = store_.notebook(notebookId_);
    if (!book || book->type != "todo" || name.isEmpty() || name == QStringLiteral("默认")) return;
    int count = 0;
    for (const auto &note: book->notes) {
        if (note.metadata["category"].toString() == name) ++count;
    }
    const auto hint = count > 0 ? QCoreApplication::translate("MainWindow", "确认删除分类「%1」？其中 %2 个事项将移至「默认」分类。").arg(name).arg(count)
                                : QCoreApplication::translate("MainWindow", "确认删除分类「%1」？").arg(name);
    if (QMessageBox::question(this, QCoreApplication::translate("MainWindow", "删除分类"), hint, QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes) return;
    QString error;
    if (!NotebookCategories::remove(*book, name, &error)) {
        QMessageBox::warning(this, QCoreApplication::translate("MainWindow", "删除分类"), error);
        return;
    }
    if (todoCategoryFilter_ == name) {
        todoCategoryFilter_.clear();
        todoPage_ = 0;
    }
    save();
    refreshTodoCategories();
}

void TodoWorkspace::showCategoryMenu(const QPoint &position) {
    const auto *item = tree_->itemAt(position);
    const auto category = item ? item->data(0, Qt::UserRole).toString() : QString();
    QMenu menu(this);
    menu.addAction(QCoreApplication::translate("MainWindow", "新建分类"), this, &TodoWorkspace::createTodoCategory);
    if (!category.isEmpty() && category != QStringLiteral("默认")) {
        menu.addAction(QCoreApplication::translate("MainWindow", "重命名分类"), this, [this, category] { renameTodoCategory(category); });
        menu.addAction(QCoreApplication::translate("MainWindow", "删除分类"), this, [this, category] { deleteTodoCategory(category); });
    }
    menu.addSeparator();
    menu.addAction(QCoreApplication::translate("MainWindow", "打开笔记本…"), this, &TodoWorkspace::openNotebookRequested);
    menu.addAction(QCoreApplication::translate("MainWindow", "关闭笔记本"), this, &TodoWorkspace::closeNotebookRequested);
    menu.exec(tree_->viewport()->mapToGlobal(position));
}
