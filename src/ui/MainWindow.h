#ifndef QSNOTE_MAINWINDOW_H
#define QSNOTE_MAINWINDOW_H

#include "core/NotebookStore.h"
#include <QMainWindow>
#include <QHash>
#include <QSet>
#include <QStringList>

class TodoWorkspace;
class EditorWorkspace;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSplitter;
class QStackedWidget;
class QTextBrowser;
class QTimer;
class QSystemTrayIcon;
class UpdateChecker;
class QTreeWidget;
class QTreeWidgetItem;
class QFileSystemWatcher;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    void maximizeWindow();
    void openFile(const QString &path);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void changeEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void buildUi();
    void refreshNotebooks();
    void refreshTree();
    void refreshDirectories();
    void watchDirectories();
    void refreshEditor();
    void updatePanels();
    void updateResizeHandles();
    void updateMaximizeButton();
    void createNotebook();
    void openNotebook();
    void closeNotebook();
    void createNote(const QString &parentId, bool folder, const QString &category = {});
    void showTreeMenu(const QPoint &position);
    void copyTreeItem(const QString &id);
    void pasteTreeFiles(const QString &id);
    void toggleMaximized();
    void showSettings();
    void applyUiSettings();
    void setupTray();
    void checkForUpdates(QLabel *status = nullptr);
    void showSiteConfig();
    void showSitePreview();
    void exportNotebook();
    void publishSite(const Notebook &book);
    bool save();
    void applyRemoval(const NotebookStore::RemovalResult &result);
    bool generateSite(const Notebook &book);

    QSystemTrayIcon *tray_ = nullptr;
    UpdateChecker *updateChecker_ = nullptr;
    bool quitting_ = false;
    NotebookStore store_;
    QString currentNotebookId_;
    QSplitter *splitter_ = nullptr;
    QWidget *header_ = nullptr;
    QHash<QWidget *, Qt::Edges> resizeHandles_;
    QWidget *left_ = nullptr;
    QWidget *right_ = nullptr;
    QComboBox *notebooks_ = nullptr;
    QComboBox *groupBy_ = nullptr;
    TodoWorkspace *todoWorkspace_ = nullptr;
    QLineEdit *search_ = nullptr;
    QTreeWidget *tree_ = nullptr;
    QFileSystemWatcher *fileWatcher_ = nullptr;
    QLabel *treeHint_ = nullptr;
    QStackedWidget *pages_ = nullptr;
    EditorWorkspace *editorWorkspace_ = nullptr;
    QPushButton *toggleLeft_ = nullptr;
    QPushButton *toggleRight_ = nullptr;
    QPushButton *siteConfig_ = nullptr;
    QPushButton *sitePreview_ = nullptr;
    QPushButton *export_ = nullptr;
    QCheckBox *autoNumberChapters_ = nullptr;
    QPushButton *maximize_ = nullptr;
    QTimer *saveTimer_ = nullptr;
    bool leftRequested_ = true;
    bool rightRequested_ = true;
    bool storageReady_ = true;
    bool maximized_ = false;
    QRect normalGeometry_;
};

#endif// QSNOTE_MAINWINDOW_H
