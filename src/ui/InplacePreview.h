#ifndef QSNOTE_INPLACEPREVIEW_H
#define QSNOTE_INPLACEPREVIEW_H

#include <QCache>
#include <QObject>
#include <QTimer>
#include <vtextedit/preview.h>
#include <vtextedit/previewmgr.h>

class MarkdownEditor;
class QWebEnginePage;

class InplacePreview : public QObject {
    Q_OBJECT
public:
    explicit InplacePreview(MarkdownEditor *editor);
    void refresh();

public slots:
    void ready();
    void rendered(int generation, int index, const QString &png, int width, int height, const QString &error);

signals:
    void renderRequested(const QString &request);
    void stylesheetReady(const QString &css);

private:
    struct Request {
        QSharedPointer<const vte::Preview> preview;
        QString language;
        QString text;
        QString key;
        bool display = false;
        QSharedPointer<vte::PreviewItem> item;
    };
    struct Image {
        QPixmap pixmap;
        QSize size;
    };
    void invalidate();
    void update(const QVector<QSharedPointer<const vte::Preview>> &previews);
    void renderNext();
    void publish();
    void attach(int index, const Image &image);
    void ensurePage();

    MarkdownEditor *editor_;
    QWebEnginePage *page_ = nullptr;
    QTimer renderTimer_;
    QTimer publishTimer_;
    QCache<QString, Image> cache_{16384};// Raster bytes in KiB, bounded to 16 MiB.
    QVector<Request> requests_;
    quint64 revision_ = 0;
    int generation_ = 0;
    int next_ = 0;
    bool ready_ = false;
    bool busy_ = false;
};

#endif
