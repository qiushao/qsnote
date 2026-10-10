#ifndef QSNOTE_MAINWINDOW_H
#define QSNOTE_MAINWINDOW_H

#include "core/NotebookStore.h"
#include <QMainWindow>
#include <QHash>
#include <QSet>
#include <QSharedPointer>

class MarkdownEditor;
class MarkdownPreview;
class QComboBox;
class QLabel;
class QLineEdit;
class QMenu;
namespace vte {
class VTextEdit;
}
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
    ~MainWindow() override;

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
    void updateStatus();
    void updatePanels();
    void createNotebook();
    void openNotebook();
    void closeNotebook();
    void createNote(const QString &parentId, bool folder);
    void showTreeMenu(const QPoint &position);
    void openNote(const QString &id);
    void closeTab(int index);
    enum class ViewMode { Edit, Preview, Split };
    struct Heading {
        QString title;
        int level = 0;
        int line = 0;
    };
    struct EditorSession {
        MarkdownEditor *editor = nullptr;
        QSharedPointer<QWidget> status;
        QMenu *menu = nullptr;
        QVector<Heading> headings;
        ViewMode mode = ViewMode::Edit;
        int verticalScroll = 0;
        int horizontalScroll = 0;
        int previewLine = 0;
    };
    void activateEditor(const QString &id);
    void removeEditor(const QString &id);
    void setPreview(bool enabled);
    void setViewMode(ViewMode mode);
    void toggleMaximized();
    void showSiteConfig();
    void showSitePreview();
    void exportNotebook();
    bool save();
    QString currentNoteId() const;
    static QString siteHtml(const Notebook &book, const QString &page);

    NotebookStore store_;
    QString currentNotebookId_;
    QSplitter *splitter_ = nullptr;
    QWidget *header_ = nullptr;
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
    QStackedWidget *editors_ = nullptr;
    QStackedWidget *editorStatuses_ = nullptr;
    QHash<QString, EditorSession> editorSessions_;
    QString activeEditorId_;
    MarkdownEditor *markdownEditor_ = nullptr;
    vte::VTextEdit *editor_ = nullptr;
    MarkdownPreview *preview_ = nullptr;
    QSplitter *editorSplitter_ = nullptr;
    QTimer *previewTimer_ = nullptr;
    ViewMode viewMode_ = ViewMode::Edit;
    bool syncingPreviewScroll_ = false;
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
    QPushButton *splitMode_ = nullptr;
    QPushButton *maximize_ = nullptr;
    QTimer *saveTimer_ = nullptr;
    bool previewModeEnabled_ = false;
    bool leftRequested_ = true;
    bool rightRequested_ = true;
    bool storageReady_ = true;
};

#endif// QSNOTE_MAINWINDOW_H
