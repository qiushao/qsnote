#ifndef QSNOTE_MAINWINDOW_H
#define QSNOTE_MAINWINDOW_H

#include "core/NotebookStore.h"
#include <QMainWindow>
#include <QHash>
#include <QSet>
#include <QStringList>

class MarkdownEditor;
class MarkdownPreview;
class QCheckBox;
class QComboBox;
class QDateEdit;
class QLabel;
class QLineEdit;
namespace vte {
class VTextEdit;
}
class QPushButton;
class QPlainTextEdit;
class QSplitter;
class QStackedWidget;
class QTabBar;
class QTextBrowser;
class QTimer;
class QSystemTrayIcon;
class UpdateChecker;
class QTreeWidget;
class QTreeWidgetItem;
class QUrl;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    void maximizeWindow();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void changeEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void buildUi();
    void buildToolbar();
    void refreshNotebooks();
    void refreshTree();
    void refreshEditor();
    void refreshOutline();
    void buildTodoUi();
    void refreshTodoCategories(const QString &selectedId = {});
    void refreshTodoList(const QString &selectedId = {});
    void refreshTodoDetails();
    void createTodo();
    void deleteTodo(const QString &noteId);
    void clearCompletedTodos();
    void createTodoCategory();
    void renameTodoCategory(const QString &name);
    void deleteTodoCategory(const QString &name);
    void updateStatus();
    void updatePanels();
    void updateResizeHandles();
    void updateMaximizeButton();
    void createNotebook();
    void openNotebook();
    void closeNotebook();
    void createNote(const QString &parentId, bool folder, const QString &category = {});
    void showTreeMenu(const QPoint &position);
    void openNote(const QString &id);
    void openExternalFile(const QString &path);
    Note *documentNote(const QString &id);
    void showTabMenu(const QPoint &position);
    void closeTab(int index);
    enum class ViewMode { Edit, Preview, Split };
    struct Heading {
        QString title;
        int level = 0;
        int line = 0;
    };
    struct EditorSession {
        MarkdownEditor *editor = nullptr;
        QVector<Heading> headings;
        ViewMode mode = ViewMode::Preview;
        int verticalScroll = 0;
        int horizontalScroll = 0;
        int previewLine = 0;
    };
    void activateEditor(const QString &id);
    void removeEditor(const QString &id);
    void setPreview(bool enabled);
    void setViewMode(ViewMode mode);
    void toggleMaximized();
    void showSettings();
    void applyUiSettings();
    void setupTray();
    void checkForUpdates(QLabel *status = nullptr);
    void showSiteConfig();
    void showSitePreview();
    void exportNotebook();
    bool save();
    bool saveEditors();
    void applyRemoval(const NotebookStore::RemovalResult &result);
    QString currentNoteId() const;
    QString noteDirectory(const QString &id);
    void openLocalLink(const QUrl &url);
    bool generateSite(const Notebook &book);

    QSystemTrayIcon *tray_ = nullptr;
    UpdateChecker *updateChecker_ = nullptr;
    bool quitting_ = false;
    NotebookStore store_;
    QHash<QString, Note> externalFiles_;
    QString currentNotebookId_;
    QSplitter *splitter_ = nullptr;
    QWidget *header_ = nullptr;
    QHash<QWidget *, Qt::Edges> resizeHandles_;
    QWidget *left_ = nullptr;
    QWidget *right_ = nullptr;
    QWidget *toolbar_ = nullptr;
    QPushButton *undo_ = nullptr;
    QPushButton *redo_ = nullptr;
    QComboBox *notebooks_ = nullptr;
    QComboBox *groupBy_ = nullptr;
    QComboBox *todoSort_ = nullptr;
    QComboBox *todoPriority_ = nullptr;
    QComboBox *todoStatus_ = nullptr;
    QWidget *todoListPage_ = nullptr;
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
    QLineEdit *search_ = nullptr;
    QTreeWidget *tree_ = nullptr;
    QLabel *treeHint_ = nullptr;
    QTabBar *tabs_ = nullptr;
    QStackedWidget *pages_ = nullptr;
    QStackedWidget *editors_ = nullptr;
    QHash<QString, EditorSession> editorSessions_;
    QString activeEditorId_;
    MarkdownEditor *markdownEditor_ = nullptr;
    vte::VTextEdit *editor_ = nullptr;
    MarkdownPreview *preview_ = nullptr;
    QSplitter *editorSplitter_ = nullptr;
    QTimer *previewTimer_ = nullptr;
    ViewMode viewMode_ = ViewMode::Preview;
    bool syncingPreviewScroll_ = false;
    QTreeWidget *outline_ = nullptr;
    QLabel *outlineHint_ = nullptr;
    QLabel *status_ = nullptr;
    QPushButton *toggleLeft_ = nullptr;
    QPushButton *toggleRight_ = nullptr;
    QPushButton *siteConfig_ = nullptr;
    QPushButton *sitePreview_ = nullptr;
    QPushButton *export_ = nullptr;
    QCheckBox *autoNumberChapters_ = nullptr;
    QPushButton *editMode_ = nullptr;
    QPushButton *previewMode_ = nullptr;
    QPushButton *splitMode_ = nullptr;
    QPushButton *maximize_ = nullptr;
    QTimer *saveTimer_ = nullptr;
    bool leftRequested_ = true;
    bool rightRequested_ = true;
    bool storageReady_ = true;
    bool maximized_ = false;
    QRect normalGeometry_;
};

#endif// QSNOTE_MAINWINDOW_H
