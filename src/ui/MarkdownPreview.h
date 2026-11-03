#ifndef QSNOTE_MARKDOWNPREVIEW_H
#define QSNOTE_MARKDOWNPREVIEW_H

#include <QWebEngineView>
#include <QPointer>

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
    void renderPlantUml(const QString &source, int revision, int id);
    QString localImage(const QString &url);
    void openLocalLink(int revision, const QString &url) { emit localLinkRequested(revision, url); }
    void diagramPng(int requestId, const QString &data) { emit diagramPngReceived(requestId, data); }
signals:
    void readyReceived();
    void renderedReceived(int revision);
    void sourceLineReceived(int revision, int line);
    void taskChangedReceived(int revision, int line, bool checked);
    void documentChanged(const QString &markdown, int revision, bool resetScroll, const QString &baseUrl);
    void scrollRequested(int revision, int line, bool center);
    void plantUmlRendered(int revision, int id, const QString &data, const QString &error);
    void localLinkRequested(int revision, const QString &url);
    void diagramPngReceived(int requestId, const QString &data);
};

class MarkdownPreview : public QWebEngineView {
    Q_OBJECT
public:
    explicit MarkdownPreview(QWidget *parent = nullptr);
    void setMarkdown(const QString &markdown, const QString &documentId, const QString &directory = {});
    void scrollToLine(int line, bool center = false);
    void scrollToAnchor(const QString &anchor);
    void showFind();
    void showSettings();
    void showHeadingNavigation();
    void setHeadingFoldingEnabled(bool enabled);
    void applyReaderSettings();
    bool matchesDocument(const QString &markdown, const QString &documentId) const {
        return markdown_ == markdown && documentId_ == documentId;
    }
    int revision() const { return revision_; }
signals:
    void sourceLineChanged(int line);
    void taskToggled(int line, bool checked);
    void localLinkActivated(const QUrl &url);

protected:
    void contextMenuEvent(QContextMenuEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    enum class Capture { None, Clipboard, File };
    void findInPreview(bool backward = false, bool restart = false);
    void showContextMenu(const QPoint &globalPos, const QString &diagramId);
    void saveDiagram(const QString &diagramId);
    void captureDiagram(const QString &diagramId, Capture mode, const QString &path = {});
    PreviewBridge *bridge_;
    QDialog *findDialog_ = nullptr;
    QLineEdit *findQuery_ = nullptr;
    QCheckBox *findCase_ = nullptr;
    QLabel *findResult_ = nullptr;
    int findRequest_ = 0;
    QPointer<QDialog> headingDialog_;
    QString markdown_;
    QString documentId_;
    QString baseUrl_;
    QString pendingAnchor_;
    int revision_ = 0;
    int renderedRevision_ = -1;
    int pendingLine_ = -1;
    bool pendingLineCentered_ = false;
    bool loading_ = false;
    bool ready_ = false;
    Capture capture_ = Capture::None;
    QString capturePath_;
    int captureId_ = 0;
};

#endif
