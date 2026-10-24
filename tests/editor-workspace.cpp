#include "core/NotebookStore.h"
#include "ui/MarkdownEditor.h"
#include <QtWidgets>
#include <vtextedit/vtextedit.h>
#define private public
#include "ui/EditorWorkspace.h"
#include "ui/MainWindow.h"
#include "ui/MarkdownPreview.h"
#undef private

namespace {
void require(bool ok, const char *message) {
    if (!ok) qFatal("%s", message);
}
void events(int ms = 200) {
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}
void write(const QString &path, const QByteArray &bytes) {
    QFile file(path);
    require(file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size(), "fixture write");
}
QByteArray read(const QString &path) {
    QFile file(path);
    require(file.open(QIODevice::ReadOnly), "fixture read");
    return file.readAll();
}
void shortcut(MainWindow &window, const QKeySequence &key) {
    for (auto *action: window.findChildren<QShortcut *>()) {
        if (action->key() == key) {
            emit action->activated();
            return;
        }
    }
    require(false, "shortcut exists");
}
void append(vte::VTextEdit *editor, const QString &text) {
    auto cursor = editor->textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(text);
}
}// namespace
int main(int argc, char **argv) {
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QApplication app(argc, argv);
    QTemporaryDir temp;
    qputenv("XDG_DATA_HOME", temp.path().toUtf8());
    qputenv("XDG_CACHE_HOME", temp.path().toUtf8());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temp.path());
    QSettings().setValue("General/checkUpdatesOnStartup", false);
    QSettings().setValue("session/notebookId", "normal");
    NotebookStore store;
    const auto longBody = QString("# Heading\n- [ ] task\n") + QString("paragraph\n\n").repeated(200);
    store.notebooks = {{"normal", "Normal", "normal", {{"one", {}, "One", longBody, false}, {"two", {}, "Two", "# Second\ntext", false}}, {}, temp.filePath("normal")}, {"todo", "Todo", "todo", {}, {}, temp.filePath("todo")}};
    QString error;
    require(store.save(&error), qPrintable(error));
    MainWindow window;
    window.resize(1200, 800);
    window.show();
    events();
    auto *workspace = window.editorWorkspace_;
    workspace->openNote("one", true);
    events();
    auto *first = workspace->editor_;
    append(first, "first edit");
    const auto firstEdited = first->toPlainText();
    auto cursor = first->textCursor();
    cursor.setPosition(40);
    cursor.setPosition(55, QTextCursor::KeepAnchor);
    first->setTextCursor(cursor);
    first->verticalScrollBar()->setValue(first->verticalScrollBar()->maximum() / 2);
    const auto scroll = first->verticalScrollBar()->value();
    require(scroll > 0 && workspace->undo_->isEnabled(), "scroll and undo available");
    workspace->openNote("two", true);
    events();
    auto *second = workspace->editor_;
    append(second, "second edit");
    workspace->setViewMode(EditorWorkspace::ViewMode::Split);
    events();
    workspace->openNote("one");
    events();
    require(workspace->editor_ == first && workspace->viewMode_ == EditorWorkspace::ViewMode::Edit, "editor lifetime and per-document mode");
    require(first->textCursor().position() == 55 && first->textCursor().anchor() == 40, "selection restored");
    require(first->verticalScrollBar()->value() == scroll, "scroll restored");
    workspace->undo_->click();
    require(first->toPlainText() == longBody && second->toPlainText().endsWith("second edit"), "independent undo");
    workspace->redo_->click();
    require(first->toPlainText() == firstEdited, "redo retained");
    workspace->openNote("two");
    events();
    require(workspace->viewMode_ == EditorWorkspace::ViewMode::Split && workspace->editor_ == second, "split restored");
    workspace->undo_->click();
    require(second->toPlainText() == "# Second\ntext", "second undo history");
    workspace->openNote("one");
    workspace->setViewMode(EditorWorkspace::ViewMode::Preview);
    events(500);
    emit workspace->preview_->taskToggled(1, true);
    require(window.store_.note("one")->content.contains("- [x] task") && workspace->previewTimer_->isActive(), "preview task edits source and schedules preview");
    events(300);
    require(workspace->preview_->matchesDocument(first->toPlainText(), "one"), "preview debounce updates document");
    workspace->preview_->setMarkdown("stale", "other");
    emit workspace->preview_->taskToggled(1, false);
    require(window.store_.note("one")->content.contains("- [x] task"), "stale preview cannot edit source");
    workspace->setViewMode(EditorWorkspace::ViewMode::Split);
    events();
    emit workspace->preview_->sourceLineChanged(80);
    events();
    require(workspace->editorSessions_["one"].previewLine == 80 && workspace->markdownEditor_->getTopLine() >= 78, "preview to source scrolling");
    QTimer::singleShot(0, [&] {
        auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        require(dialog && dialog->objectName() == "findReplace", "replace shortcut opens editor dialog");
        dialog->findChild<QLineEdit *>("findText")->setText("first edit");
        dialog->findChild<QLineEdit *>("replaceText")->setText("replaced");
        dialog->findChild<QPushButton *>("findAction3")->click();
        dialog->accept();
    });
    shortcut(window, QKeySequence::Replace);
    require(first->toPlainText().endsWith("replaced"), "replace changes active document");
    workspace->setViewMode(EditorWorkspace::ViewMode::Preview);
    shortcut(window, QKeySequence::Find);
    auto *find = workspace->preview_->findChild<QDialog *>("previewFind");
    require(find && find->isVisible(), "preview find routing");
    find->close();
    window.notebooks_->setCurrentIndex(window.notebooks_->findData("todo"));
    shortcut(window, QKeySequence::Find);
    require(workspace->currentNoteId().isEmpty() && window.search_->hasFocus(), "todo find routes to common search");
    require(!workspace->previewTimer_->isActive() && workspace->openDocuments().size() == 2, "todo suspends preview and retains sessions");
    window.notebooks_->setCurrentIndex(window.notebooks_->findData("normal"));
    const auto path = temp.filePath("external.md");
    write(path, QByteArray("\xef\xbb\xbf") + "# External\r\nbody\r\n");
    workspace->openExternalFile(path);
    workspace->setPreview(false);
    const auto externalId = workspace->currentNoteId();
    require(workspace->editor_->toPlainText() == "# External\nbody\n", "external BOM/CRLF decoded");
    append(workspace->editor_, "new\nline");
    require(window.save(), "external save");
    require(read(path) == QByteArray("\xef\xbb\xbf") + "# External\r\nbody\r\nnew\r\nline", "external BOM/CRLF preserved");
    const auto count = workspace->tabs_->count();
    window.storageReady_ = false;
    QTimer::singleShot(0, [] { auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget()); require(box, "save failure warning"); box->accept(); });
    workspace->closeTab(workspace->tabs_->currentIndex());
    require(workspace->tabs_->count() == count && workspace->currentNoteId() == externalId, "save failure prevents tab close");
    window.storageReady_ = true;
    QPointer<MarkdownEditor> externalEditor = workspace->activeEditor();
    workspace->closeTab(workspace->tabs_->currentIndex());
    require(!externalEditor && !workspace->externalFiles_.contains(externalId), "closing releases editor and external state");
    workspace->openExternalFile(path);
    // Tab context menu retains only its target, saving before removing other sessions.
    const auto target = workspace->tabs_->tabRect(workspace->tabs_->currentIndex()).center();
    QTimer::singleShot(0, [] {
        auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
        require(menu, "tab menu");
        for (auto *action: menu->actions())
            if (action->text() == "关闭其他标签") {
                action->trigger();
                menu->close();
                return;
            }
        require(false, "close other tabs action");
    });
    workspace->showTabMenu(target);
    require(workspace->openDocuments() == QStringList{externalId} && workspace->editorSessions_.size() == 1, "close other tabs and sessions");
    workspace->openNote("one");
    workspace->tabs_->moveTab(1, 0);
    workspace->openExternalFile(path);
    const auto saved = workspace->openDocuments();
    require(window.close(), "close saves session");
    MainWindow restored;
    restored.resize(1200, 800);
    restored.show();
    events(500);
    require(restored.editorWorkspace_->openDocuments() == saved && restored.editorWorkspace_->currentNoteId() == externalId, "restore order and external active document");
    require(restored.editorWorkspace_->editor_->toPlainText() == "# External\nbody\nnew\nline", "restored external body");
    qInfo() << "PASS: independent undo/modes/selection/scroll, task and stale preview, search/replace, todo suspension, BOM/CRLF, close failure/lifetime/menu, session restore";
}
