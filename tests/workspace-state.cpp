#include "core/NotebookStore.h"
#include "ui/MarkdownEditor.h"
#include <QtWidgets>
#include <vtextedit/vtextedit.h>
#define private public
#include "ui/MainWindow.h"
#include "ui/MarkdownPreview.h"
#undef private
void require(bool ok, const char *message) {
    if (!ok) qFatal("%s", message);
}
void events(int ms = 200) {
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}
int main(int argc, char **argv) {
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QApplication app(argc, argv);
    QTemporaryDir temp;
    qputenv("XDG_DATA_HOME", temp.path().toUtf8());
    qputenv("XDG_CACHE_HOME", temp.path().toUtf8());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temp.path());
    QSettings settings;
    settings.setValue("General/checkUpdatesOnStartup", false);
    settings.setValue("session/notebookId", "normal");
    settings.setValue("session/noteId", "note");
    settings.setValue("session/openNotes", QStringList{"note"});
    NotebookStore store;
    store.notebooks = {{"normal", "Normal", "normal", {{"folder", {}, "Folder", {}, true}, {"note", {}, "Note", "# Heading\n\nBody\n", false}, {"child", "folder", "Child", {}, false}}, {}, temp.filePath("normal")}, {"todo", "Todo", "todo", {}, {}, temp.filePath("todo")}};
    for (int i = 0; i < 45; ++i) store.notebooks.last().notes.append({QString::number(i), {}, QString::number(i), {}, false, {{"priority", 2}, {"category", "默认"}}});
    QString error;
    require(store.save(&error), qPrintable(error));
    MainWindow window;
    window.resize(1200, 800);
    window.show();
    events(1000);
    require(window.currentNoteId() == "note" && window.tabs_->count() == 1, "restore current note and tabs");
    require(window.outline_->topLevelItemCount() == 1, "restore outline");
    window.tree_->topLevelItem(0)->setExpanded(false);
    window.refreshNotebooks();
    require(!window.tree_->topLevelItem(0)->isExpanded(), "preserve collapsed folder");
    require(window.tree_->currentItem()->data(0, Qt::UserRole) == "note", "preserve tree selection");
    for (auto mode: {MainWindow::ViewMode::Edit, MainWindow::ViewMode::Preview, MainWindow::ViewMode::Split}) {
        window.setViewMode(mode);
        events(300);
        require(window.editors_->isVisible() == (mode != MainWindow::ViewMode::Preview), "editor visibility");
        require(window.preview_->isVisible() == (mode != MainWindow::ViewMode::Edit), "preview visibility");
        auto *heading = window.outline_->topLevelItem(0);
        emit window.outline_->itemClicked(heading, 0);
        require(!window.status_->text().isEmpty(), "status text");
    }
    QShortcut *toggle = nullptr;
    for (auto *shortcut: window.findChildren<QShortcut *>())
        if (shortcut->key() == QKeySequence("Ctrl+E")) toggle = shortcut;
    require(toggle, "Ctrl+E shortcut");
    emit toggle->activated();
    require(window.viewMode_ == MainWindow::ViewMode::Preview, "split to preview shortcut");
    emit toggle->activated();
    require(window.viewMode_ == MainWindow::ViewMode::Edit, "preview to edit shortcut");
    auto *preview = window.preview_;
    preview->setMarkdown("new", "new");
    const int revision = preview->revision();
    const int before = preview->renderedRevision_;
    preview->bridge_->rendered(revision - 2);
    require(preview->renderedRevision_ == before, "reject stale render");
    preview->bridge_->rendered(revision);
    require(preview->renderedRevision_ == revision, "accept current render");
    window.notebooks_->setCurrentIndex(window.notebooks_->findData("todo"));
    events();
    window.search_->setText("missing");
    QTimer::singleShot(20, &window, [&] {
        auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        require(dialog, "create todo dialog");
        dialog->findChild<QLineEdit *>("newTodoTitle")->setText("New todo");
        dialog->accept();
    });
    window.createTodo();
    require(window.todoPage_ == 2 && window.todoList_->currentItem()->text(0) == "New todo", "new todo selected on its page");
    require(window.search_->text().isEmpty() && window.todoTitle_->text() == "New todo", "new todo details and search");
    require(window.tabs_->count() == 1, "todo preserves Markdown tabs");
    QTimer::singleShot(20, &window, [] {
        auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
        require(box, "close notebook confirmation");
        box->button(QMessageBox::Yes)->click();
    });
    window.closeNotebook();
    events();
    require(window.currentNoteId() == "note" && window.tabs_->count() == 1, "close todo restores Markdown workspace");
    qInfo() << "PASS: startup restore, tree state, modes/shortcut/outline, stale revisions, paginated todo creation, close notebook";
}
