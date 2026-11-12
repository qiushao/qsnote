#pragma once

#include "core/TextSearch.h"
#include <QDialog>
#include <QTextCursor>
#include <functional>

class QCheckBox;
class QComboBox;
class QLabel;
class QPlainTextEdit;
class QTextEdit;
class QTabBar;

class FindReplaceDialog : public QDialog {
    Q_OBJECT
public:
    static void openFor(QPlainTextEdit *editor, bool replace = false);
    static void openFor(QTextEdit *editor, bool replace = false);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    FindReplaceDialog(QWidget *editor, QTextDocument *document, std::function<QTextCursor()> cursor, std::function<void(const QTextCursor &)> select);
    template<typename Editor> static void openEditor(Editor *editor, bool replace);
    void present(bool replace);
    TextSearch::Options options() const;
    bool collect();
    void find(bool backward);
    void replace(bool all);
    void showMatches();
    void selectMatch(int index);
    static void remember(QComboBox *field);
    void saveHistory();
    void invalidate();

    QWidget *editor_;
    QTextDocument *document_;
    std::function<QTextCursor()> cursor_;
    std::function<void(const QTextCursor &)> select_;
    QComboBox *query_;
    QComboBox *replacement_;
    QComboBox *mode_;
    QTabBar *tabs_;
    QCheckBox *case_;
    QCheckBox *whole_;
    QCheckBox *wrap_;
    QCheckBox *selectionOnly_;
    QCheckBox *dot_;
    QLabel *status_;
    QTextCursor scope_;
    QVector<QRegularExpressionMatch> matches_;
    int lastStart_ = -1;
    int lastEnd_ = -1;
};
