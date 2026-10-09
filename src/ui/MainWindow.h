#ifndef QSNOTE_MAINWINDOW_H
#define QSNOTE_MAINWINDOW_H

#include "core/NotebookStore.h"
#include <QMainWindow>
#include <QSet>

class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSplitter;
class QStackedWidget;
class QTabBar;
class QTextBrowser;
class QTimer;
class QTreeWidget;
class QTreeWidgetItem;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    void buildUi();
    void buildToolbar();
    void refreshNotebooks();
    void refreshTree();
    void refreshEditor();
    void refreshOutline();
    void updateStatus();
    void updatePanels();
    void createNotebook();
    void createNote(const QString &parentId, bool folder);
    void showTreeMenu(const QPoint &position);
    void openNote(const QString &id);
    void closeTab(int index);
    void setPreview(bool enabled);
    void insertMarkdown(const QString &before, const QString &after, const QString &placeholder, bool block);
    void showSiteConfig();
    void showSitePreview();
    void exportNotebook();
    bool save();
    QString currentNoteId() const;
    static QString siteHtml(const Notebook &book, const QString &page);

    NotebookStore store_;
    QString currentNotebookId_;
    QSplitter *splitter_ = nullptr;
    QWidget *left_ = nullptr;
    QWidget *right_ = nullptr;
    QWidget *toolbar_ = nullptr;
    QWidget *metadata_ = nullptr;
    QComboBox *notebooks_ = nullptr;
    QComboBox *groupBy_ = nullptr;
    QLineEdit *search_ = nullptr;
    QTreeWidget *tree_ = nullptr;
    QLabel *treeHint_ = nullptr;
    QTabBar *tabs_ = nullptr;
    QStackedWidget *pages_ = nullptr;
    QPlainTextEdit *editor_ = nullptr;
    QTextBrowser *preview_ = nullptr;
    QTreeWidget *outline_ = nullptr;
    QLabel *outlineHint_ = nullptr;
    QLabel *status_ = nullptr;
    QPushButton *toggleLeft_ = nullptr;
    QPushButton *toggleRight_ = nullptr;
    QPushButton *siteConfig_ = nullptr;
    QPushButton *sitePreview_ = nullptr;
    QPushButton *export_ = nullptr;
    QPushButton *editMode_ = nullptr;
    QPushButton *previewMode_ = nullptr;
    QTimer *saveTimer_ = nullptr;
    bool previewModeEnabled_ = false;
    bool leftRequested_ = true;
    bool rightRequested_ = true;
    bool storageReady_ = true;
};

#endif// QSNOTE_MAINWINDOW_H
