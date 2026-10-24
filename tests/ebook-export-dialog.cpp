#include "core/EbookExporter.h"
#include "ui/EbookExportDialog.h"
#include "ui/MarkdownEditor.h"
#include <QtWidgets>
#include <vtextedit/vtextedit.h>
#define private public
#include "ui/MainWindow.h"
#undef private

namespace {
void require(bool condition, const QString &message) {
    if (!condition) qFatal("%s", qPrintable(message));
}
QByteArray read(const QString &path) {
    QFile file(path);
    require(file.open(QIODevice::ReadOnly), file.errorString());
    return file.readAll();
}
QByteArray run(const QString &program, const QStringList &args) {
    QProcess process;
    process.start(program, args);
    require(process.waitForFinished(30000) && process.exitCode() == 0, "artifact check: " + program);
    return process.readAllStandardOutput();
}
void choose(EbookExportDialog *dialog, EbookExporter::Format format, bool current, const QString &directory) {
    auto *scope = dialog->findChild<QComboBox *>("exportScope");
    require(scope->count() == 2, "whole book and current document scopes");
    scope->setCurrentIndex(current ? 1 : 0);
    auto *choice = dialog->findChild<QComboBox *>("exportFormat");
    choice->setCurrentIndex(choice->findData(int(format)));
    dialog->findChild<QLineEdit *>("exportDirectory")->setText(directory);
    auto *confirm = dialog->findChild<QPushButton *>("confirmExport");
    require(confirm->isEnabled(), "valid export form");
    confirm->click();
}
}// namespace
int main(int argc, char **argv) {
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QApplication app(argc, argv);
    QTemporaryDir temp;
    require(temp.isValid(), "temporary directory");
    qputenv("XDG_DATA_HOME", temp.path().toUtf8());
    qputenv("XDG_CACHE_HOME", temp.path().toUtf8());
    qputenv("XDG_CONFIG_HOME", temp.filePath("config").toUtf8());
    QDir().mkpath(temp.filePath("config"));
    QFile dirs(temp.filePath("config/user-dirs.dirs"));
    require(dirs.open(QIODevice::WriteOnly), "documents fixture");
    dirs.write(("XDG_DOCUMENTS_DIR=\"" + temp.filePath("documents") + "\"\n").toUtf8());
    dirs.close();
    require(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation).startsWith(temp.path()), "isolated Documents location");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temp.path());
    QSettings().setValue("General/checkUpdatesOnStartup", false);
    QDir().mkpath(temp.filePath("exports"));
    const auto directory = temp.filePath("exports");
    NotebookStore store;
    QString error;
    for (const auto &type: {QString("normal"), QString("ebook")}) {
        store.notebooks.append({type, "Book-" + type, type, {{type + "-current", {}, "Current-" + type, "SAVED original", false}, {type + "-other", {}, "Other", "OTHER chapter", false}}, {}, temp.filePath(type)});
    }
    require(store.save(&error), error);
    QSettings().setValue("session/notebookId", "normal");
    MainWindow window;
    window.resize(1200, 800);
    window.show();
    const bool chmAvailable = !EbookExporter::chmCompiler().isEmpty();
    for (const auto &type: {QString("normal"), QString("ebook")}) {
        window.openNote(type + "-current");
        window.setViewMode(MainWindow::ViewMode::Edit);
        auto cursor = window.editor_->textCursor();
        cursor.select(QTextCursor::Document);
        cursor.insertText("UNSAVED snapshot body");
        window.saveTimer_->stop();
        auto *book = window.store_.notebook(type);
        require(window.store_.note(type + "-current")->content.contains("UNSAVED snapshot body"), "editor synchronously updates notebook");
        const auto originalPath = book->path + '/' + window.store_.note(type + "-current")->path;
        QString firstHtml;
        for (bool current: {false, true}) {
            for (auto format: {EbookExporter::Format::Html, EbookExporter::Format::Epub, EbookExporter::Format::Pdf, EbookExporter::Format::Chm}) {
                if (format == EbookExporter::Format::Chm && !chmAvailable) continue;
                EbookExportDialog dialog(*book, type + "-current", &window);
                QTimer::singleShot(0, [&] { choose(&dialog, format, current, directory); });
                const auto path = dialog.run();
                require(!path.isEmpty() && QFileInfo(path).size() > 0, "export output");
                QByteArray text;
                if (format == EbookExporter::Format::Html) {
                    text = read(path);
                    if (!current) firstHtml = path;
                }
                if (format == EbookExporter::Format::Epub) text = run("unzip", {"-p", path, "*book.xhtml"});
                if (format == EbookExporter::Format::Pdf) {
                    require(read(path).startsWith("%PDF"), "PDF signature");
                    text = run("pdftotext", {path, "-"});
                }
                if (format == EbookExporter::Format::Chm) require(read(path).startsWith("ITSF"), "CHM compiled artifact");
                else
                    require(text.contains("UNSAVED snapshot body") && text.contains("OTHER chapter") == !current, "scope and unsaved snapshot in output");
                require(read(originalPath) == "SAVED original", "export must not force-save source");
            }
        }
        EbookExportDialog duplicate(*book, type + "-current", &window);
        QTimer::singleShot(0, [&] { choose(&duplicate, EbookExporter::Format::Html, false, directory); });
        const auto extra = duplicate.run();
        require(extra != firstHtml && QFileInfo::exists(firstHtml), "same filename adds suffix");
        EbookExportDialog cancelled(*book, type + "-current", &window);
        require(cancelled.findChild<QLineEdit *>("exportDirectory")->text() == directory, "remember directory");
        auto *destination = cancelled.findChild<QLineEdit *>("exportDirectory");
        destination->setText(temp.filePath("missing"));
        require(!cancelled.findChild<QPushButton *>("confirmExport")->isEnabled(), "invalid directory disabled");
        const auto previousPath = qgetenv("PATH");
        qputenv("PATH", temp.filePath("missing-tools").toUtf8());
        auto *format = cancelled.findChild<QComboBox *>("exportFormat");
        format->setCurrentIndex(format->findData(int(EbookExporter::Format::Chm)));
        require(!cancelled.findChild<QPushButton *>("confirmExport")->isEnabled() && cancelled.findChild<QLabel *>("exportHint")->text().contains("chmcmd"), "missing CHM compiler feedback");
        qputenv("PATH", previousPath);
        QTimer::singleShot(0, &cancelled, &QDialog::reject);
        require(cancelled.run().isEmpty(), "cancel export");
        require(QSettings().value("ebookExport/directory") == directory, "cancel does not change remembered directory");
    }
    // The main-window export entry still dispatches to the dialog and updates status.
    window.currentNotebookId_ = "ebook";
    window.refreshNotebooks();
    QTimer::singleShot(0, [&] {
        auto *dialog = dynamic_cast<EbookExportDialog *>(QApplication::activeModalWidget());
        require(dialog, "main window export dispatch");
        choose(dialog, EbookExporter::Format::Html, true, directory);
    });
    window.exportNotebook();
    require(window.status_->text().contains(directory), "export status");
    window.store_.notebooks.append({"site", "Site", "site", {}, {}, temp.filePath("site")});
    window.currentNotebookId_ = "site";
    bool publishWarning = false, siteConfig = false;
    QTimer dialogs;
    QObject::connect(&dialogs, &QTimer::timeout, [&] {
        auto *active = QApplication::activeModalWidget();
        if (auto *box = qobject_cast<QMessageBox *>(active)) {
            publishWarning = true;
            box->accept();
        } else if (active && active->objectName() == "siteConfigDialog") {
            siteConfig = true;
            static_cast<QDialog *>(active)->accept();
        }
    });
    dialogs.start(10);
    window.exportNotebook();
    require(publishWarning && siteConfig, "site dispatch preserves publish validation/configuration");
    qInfo() << "PASS: normal/ebook whole/current HTML/EPUB/PDF, CHM available=" << chmAvailable << ", unsaved snapshot, suffix, directory/cancel, missing compiler, main dispatch";
}
