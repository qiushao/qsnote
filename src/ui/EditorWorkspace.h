#ifndef QSNOTE_EDITORWORKSPACE_H
#define QSNOTE_EDITORWORKSPACE_H

#include "core/NotebookStore.h"
#include "core/SourceOutline.h"
#include <QHash>
#include <QPixmap>
#include <QWidget>
#include <functional>

class MarkdownEditor;
class MarkdownPreview;
class QTabBar;
class QStackedWidget;
class QSplitter;
class QTimer;
class QTreeWidget;
class QLabel;
class QPushButton;
class QUrl;
class QLineEdit;
class QPlainTextEdit;
class QScrollArea;
template<typename T>
class QFutureWatcher;
namespace vte {
class VTextEdit;
}

class EditorWorkspace : public QWidget {
    Q_OBJECT
public:
    EditorWorkspace(NotebookStore &store, std::function<bool()> save, QWidget *parent = nullptr);
    QWidget *outlinePanel() const { return outlinePanel_; }
    MarkdownEditor *activeEditor() const { return markdownEditor_; }
    QString currentNoteId() const;
    QStringList openDocuments() const;
    void openNote(const QString &id, bool edit = false);
    void openExternalFile(const QString &path);
    void restoreSession(const QStringList &ids, const QString &currentId);
    void removeDocuments(const QSet<QString> &ids);
    void refresh(bool suspended);
    bool saveExternalFilesAndSync();
    void reloadSettings();
    void showStatus(const QString &text);

signals:
    void currentDocumentChanged();
    void saveRequested();
    void searchRequested();
    void notebookRequested(const QString &id);
    void locateRequested(const QString &id);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    enum class ViewMode { Edit,
                          Preview,
                          Split };
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
    void buildToolbar();
    Note *documentNote(const QString &id);
    QString noteDirectory(const QString &id);
    void openLocalLink(const QUrl &url);
    void showTabMenu(const QPoint &position);
    void closeTab(int index);
    void activateEditor(const QString &id);
    void deactivateEditor();
    void activatePlainText(const QString &id);
    void removeEditor(const QString &id);
    void setPreview(bool enabled);
    void setViewMode(ViewMode mode);
    void refreshOutline();
    void applyOutlineFilter();
    void showSourceOutline(const SourceOutline::Result &result);
    void updateStatus();
    void showEncodingMenu();
    void showLineEndingMenu();
    void changeEncoding(const QByteArray &encoding, bool reload);
    void updateImage();
    NotebookStore &store_;
    std::function<bool()> save_;
    QHash<QString, Note> externalFiles_;
    bool suspended_ = false;
    QWidget *outlinePanel_ = nullptr;
    QWidget *toolbar_ = nullptr;
    QPushButton *undo_ = nullptr;
    QPushButton *redo_ = nullptr;
    QTabBar *tabs_ = nullptr;
    QStackedWidget *pages_ = nullptr;
    QStackedWidget *editors_ = nullptr;
    QHash<QString, EditorSession> editorSessions_;
    QHash<QString, QPlainTextEdit *> textEditors_;
    QHash<QString, QSet<QString>> collapsedSourceSymbols_;
    QScrollArea *imageView_ = nullptr;
    QWidget *image_ = nullptr;
    QPixmap imagePixmap_;
    QString imageId_;
    qreal imageScale_ = 0;// Zero fits the viewport without enlarging small images.
    QString activeEditorId_;
    MarkdownEditor *markdownEditor_ = nullptr;
    vte::VTextEdit *editor_ = nullptr;
    MarkdownPreview *preview_ = nullptr;
    QSplitter *editorSplitter_ = nullptr;
    QTimer *previewTimer_ = nullptr;
    QTimer *outlineTimer_ = nullptr;
    QFutureWatcher<SourceOutline::Result> *sourceOutlineWatcher_ = nullptr;
    QString sourceOutlineId_;
    QString sourceOutlinePath_;
    QString sourceOutlineContent_;
    ViewMode viewMode_ = ViewMode::Preview;
    bool syncingPreviewScroll_ = false;
    QTreeWidget *outline_ = nullptr;
    QLineEdit *outlineFilter_ = nullptr;
    QLabel *outlineHint_ = nullptr;
    QLabel *status_ = nullptr;
    QPushButton *encoding_ = nullptr;
    QPushButton *lineEnding_ = nullptr;
    QPushButton *editMode_ = nullptr;
    QPushButton *previewMode_ = nullptr;
    QPushButton *splitMode_ = nullptr;
};

#endif
