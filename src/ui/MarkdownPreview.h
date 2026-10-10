#ifndef QSNOTE_MARKDOWNPREVIEW_H
#define QSNOTE_MARKDOWNPREVIEW_H

#include <QWebEngineView>

class QCheckBox;
class QDialog;
class QLabel;
class QLineEdit;

class PreviewBridge : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
public slots:
    void ready() { emit readyReceived(); }
    void rendered(int revision) { emit renderedReceived(revision); }
    void sourceLine(int revision, int line) { emit sourceLineReceived(revision, line); }
    void taskChanged(int revision, int line, bool checked) { emit taskChangedReceived(revision, line, checked); }
signals:
    void readyReceived();
    void renderedReceived(int revision);
    void sourceLineReceived(int revision, int line);
    void taskChangedReceived(int revision, int line, bool checked);
    void documentChanged(const QString &markdown, int revision, bool resetScroll);
    void scrollRequested(int revision, int line);
};

class MarkdownPreview : public QWebEngineView {
    Q_OBJECT
public:
    explicit MarkdownPreview(QWidget *parent = nullptr);
    void setMarkdown(const QString &markdown, const QString &documentId);
    void scrollToLine(int line);
    void showFind();
    void setHeadingFoldingEnabled(bool enabled);
    bool matchesDocument(const QString &markdown, const QString &documentId) const {
        return markdown_ == markdown && documentId_ == documentId;
    }
    int revision() const { return revision_; }
    int renderedRevision() const { return renderedRevision_; }
signals:
    void renderFinished();
    void sourceLineChanged(int line);
    void taskToggled(int line, bool checked);

protected:
    void contextMenuEvent(QContextMenuEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    void findInPreview(bool backward = false, bool restart = false);
    PreviewBridge *bridge_;
    QDialog *findDialog_ = nullptr;
    QLineEdit *findQuery_ = nullptr;
    QCheckBox *findCase_ = nullptr;
    QLabel *findResult_ = nullptr;
    int findRequest_ = 0;
    QString markdown_;
    QString documentId_;
    int revision_ = 0;
    int renderedRevision_ = -1;
    int pendingLine_ = -1;
    bool loading_ = false;
    bool ready_ = false;
};

#endif
