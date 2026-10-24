#pragma once

#include "core/NotebookStore.h"
#include <QWidget>

class QComboBox;
class QTreeWidget;
class QPushButton;
class QLabel;
class QLineEdit;
class QCheckBox;
class QDateEdit;
class QPlainTextEdit;

class TodoWorkspace : public QWidget {
    Q_OBJECT
public:
    explicit TodoWorkspace(NotebookStore &store, QWidget *parent = nullptr);
    QWidget *categoryPanel() const { return categoryPanel_; }
    QWidget *detailsPanel() const { return todoDetails_; }
    // Set the notebook/search before refreshing, so one refresh covers a coordinated switch.
    void setNotebook(const QString &id, bool storageReady);
    void setSearch(const QString &query);
    void refresh();

signals:
    void saveRequested(bool delayed);
    void removalApplied(const NotebookStore::RemovalResult &result);
    void searchCleared();
    void openNotebookRequested();
    void closeNotebookRequested();

private:
    void buildUi();
    void refreshTodoCategories(const QString &selectedId = {});
    void refreshTodoList(const QString &selectedId = {});
    void refreshTodoDetails();
    void createTodo();
    void deleteTodo(const QString &noteId);
    void clearCompletedTodos();
    void createTodoCategory();
    void renameTodoCategory(const QString &name);
    void deleteTodoCategory(const QString &name);
    void showCategoryMenu(const QPoint &position);
    void save() { emit saveRequested(false); }
    void applyRemoval(const NotebookStore::RemovalResult &result) { emit removalApplied(result); }

    NotebookStore &store_;
    QString notebookId_;
    QString query_;
    bool storageReady_ = true;
    QWidget *categoryPanel_ = nullptr;
    QTreeWidget *tree_ = nullptr;
    QComboBox *todoSort_ = nullptr;
    QComboBox *todoPriority_ = nullptr;
    QComboBox *todoStatus_ = nullptr;
    QPushButton *todoAdd_ = nullptr;
    QPushButton *todoClearCompleted_ = nullptr;
    QTreeWidget *todoList_ = nullptr;
    QLabel *todoHint_ = nullptr;
    QWidget *todoPagination_ = nullptr;
    QPushButton *todoPreviousPage_ = nullptr;
    QPushButton *todoNextPage_ = nullptr;
    QLabel *todoPageInfo_ = nullptr;
    int todoPage_ = 0;
    QWidget *todoDetails_ = nullptr;
    QLineEdit *todoTitle_ = nullptr;
    QLabel *todoCreated_ = nullptr;
    QComboBox *todoCategory_ = nullptr;
    QCheckBox *todoCompleted_ = nullptr;
    QCheckBox *todoDueEnabled_ = nullptr;
    QDateEdit *todoDue_ = nullptr;
    QPlainTextEdit *todoContent_ = nullptr;
    QString todoNotebookId_;
    QString todoCategoryFilter_;
};
