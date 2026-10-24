#pragma once

#include <QCoreApplication>
#include <QDialog>

struct Notebook;
class QComboBox;
class QLineEdit;

class EbookExportDialog : public QDialog {
    Q_DECLARE_TR_FUNCTIONS(MainWindow)
public:
    EbookExportDialog(const Notebook &book, const QString &noteId, QWidget *parent);
    // Snapshots the supplied notebook after the form is accepted, including unsaved edits.
    QString run();

private:
    const Notebook &book_;
    QComboBox *scope_;
    QComboBox *format_;
    QLineEdit *directory_;
};
