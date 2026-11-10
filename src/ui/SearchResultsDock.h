#pragma once

#include <QDockWidget>
#include <QHash>
#include <QPointer>
#include <QRegularExpression>
#include <QSet>

class QMainWindow;
class QTextDocument;
class QTabWidget;
class QTreeWidgetItem;

class SearchResultsDock : public QDockWidget {
    Q_OBJECT
public:
    explicit SearchResultsDock(QMainWindow *window);
    void showResults(const QString &query, const QString &file, QTextDocument *document, const QVector<QRegularExpressionMatch> &matches);

signals:
    void matchActivated(QTextDocument *document, int start, int end);

private:
    void invalidateDocument(QTextDocument *document);
    void closeTab(int index);
    QTabWidget *tabs_;
    QHash<QTreeWidgetItem *, QPointer<QTextDocument>> documents_;
    QSet<QTextDocument *> watchedDocuments_;
};
