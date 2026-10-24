#include "core/NotebookStore.h"
#include <QtWidgets>
#define private public
#include "ui/MainWindow.h"
#include "ui/TodoWorkspace.h"
#undef private

namespace {
void require(bool ok, const QString &message) {
    if (!ok) qFatal("%s", qPrintable(message));
}
QByteArray read(const QString &path) {
    QFile file(path);
    require(file.open(QIODevice::ReadOnly), file.errorString());
    return file.readAll();
}
void write(const QString &path, const QByteArray &bytes) {
    require(QDir().mkpath(QFileInfo(path).absolutePath()), "fixture directory");
    QFile file(path);
    require(file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size(), "fixture write");
}
}// namespace

int main(int argc, char **argv) {
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QApplication app(argc, argv);
    QTemporaryDir temp;
    require(temp.isValid(), "temporary directory");
    qputenv("XDG_DATA_HOME", temp.path().toUtf8());
    qputenv("XDG_CACHE_HOME", temp.path().toUtf8());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temp.path());
    QSettings().setValue("General/checkUpdatesOnStartup", false);
    QString error;
    for (const auto &type: {QString("normal"), QString("ebook"), QString("site"), QString("todo")}) {
        NotebookStore store;
        store.notebooks.append({type, type, type, {{"parent", {}, "Parent", "Parent body", type == "normal"}, {"keep", {}, "Keep", "Keep body", false}}, {}, temp.filePath(type)});
        if (type == "normal" || type == "ebook") {
            store.notebooks.first().notes.append({"child", "parent", "Child", "Child body", false});
            store.notebooks.first().notes.append({"grandchild", "child", "Grandchild", "Nested body", false});
            if (type == "normal") store.notebooks.first().notes[2].folder = true;
        }
        require(store.save(&error), error);
        const auto original = store.notebooks.first();
        const auto keepPath = original.path + '/' + store.note("keep")->path;
        const auto keepBytes = read(keepPath);
        QString imagePath;
        if (type == "ebook") {
            imagePath = QFileInfo(original.path + '/' + store.note("grandchild")->path).absolutePath() + "/resources/images/test.png";
            write(imagePath, "image fixture");
        }
        const auto result = store.removeNotes(type, {"parent", "not-in-this-book"});
        require(result.saved && result.error.isEmpty(), result.error);
        require(result.removedIds == (type == "normal" || type == "ebook" ? QSet<QString>{"parent", "child", "grandchild"} : QSet<QString>{"parent"}), "descendant IDs");
        require(store.notebooks.first().notes.size() == 1 && store.note("keep"), "keep unrelated note");
        require(read(keepPath) == keepBytes, "keep unrelated bytes");
        for (const auto &note: original.notes) {
            if (note.id != "keep") require(!QFileInfo::exists(original.path + '/' + note.path), "remove note files: " + note.path);
        }
        if (!imagePath.isEmpty()) require(!QFileInfo::exists(imagePath), "remove ebook child directory resources");
        Notebook reloaded;
        require(NotebookStore::readNotebook(original.path, reloaded, &error) && reloaded.notes.size() == 1, "reload deletion");
    }
    NotebookStore store;
    store.notebooks.append({"todo", "Todo", "todo", {}, {}, temp.filePath("batch")});
    for (int i = 0; i < 47; ++i) {
        store.notebooks.first().notes.append({QString::number(i), {}, QString::number(i), "Body", false, {{"completed", i % 2 == 0}, {"category", i % 3 == 0 ? "A" : "B"}}});
    }
    require(store.save(&error), error);
    const auto batch = store.removeCompletedTodos("todo");
    require(batch.saved && batch.error.isEmpty() && batch.removedIds.size() == 24, "batch spans categories and pages");
    for (const auto &note: store.notebooks.first().notes) require(!note.metadata["completed"].toBool(), "batch keeps incomplete items");

    // A failed index save leaves original bodies untouched and reports in-memory removals.
    const auto root = store.notebooks.first().path;
    const auto victim = store.notebooks.first().notes.first();
    const auto path = root + '/' + victim.path;
    const auto beforeIndex = read(root + "/_index.json");
    const auto beforeBody = read(path);
    require(QFile::setPermissions(root, QFileDevice::ReadOwner | QFileDevice::ExeOwner), "lock index directory");
    auto result = store.removeNotes("todo", {victim.id});
    require(!result.saved && !result.error.isEmpty() && result.removedIds.contains(victim.id), "save failure result");
    require(read(path) == beforeBody && read(root + "/_index.json") == beforeIndex, "save failure preserves disk");
    require(QFile::setPermissions(root, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner), "unlock index directory");

    // A file cleanup failure happens after the index was saved; retained files are reported.
    const auto next = store.notebooks.first().notes.first();
    const auto nextPath = root + '/' + next.path;
    const auto directory = QFileInfo(nextPath).absolutePath();
    require(QFile::setPermissions(directory, QFileDevice::ReadOwner | QFileDevice::ExeOwner), "lock body directory");
    result = store.removeNotes("todo", {next.id});
    require(result.saved && !result.error.isEmpty() && QFileInfo::exists(nextPath), "cleanup failure reported after saving");
    require(!read(root + "/_index.json").contains(('"' + next.id + '"').toUtf8()), "saved index excludes removed item");
    require(QFile::setPermissions(directory, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner), "unlock body directory");

    // UI closes all removed descendants, retains unrelated sessions, and saves external documents.
    store.notebooks = {{"ui", "UI", "normal", {{"folder", {}, "Folder", {}, true}, {"child", "folder", "Child", "Body", false}, {"keep", {}, "Keep", "Keep", false}}, {}, temp.filePath("ui")}};
    require(store.save(&error), error);
    QSettings().setValue("session/notebookId", "ui");
    MainWindow window;
    window.show();
    window.openNote("child");
    const auto external = temp.filePath("external.md");
    write(external, "Original");
    window.openExternalFile(external);
    window.externalFiles_.begin()->content = "Updated";
    window.externalFiles_.begin()->contentDirty = true;
    window.openNote("keep");
    window.applyRemoval(window.store_.removeNotes("ui", {"folder"}));
    window.refreshEditor();
    require(window.tabs_->count() == 2 && window.currentNoteId() == "keep" && !window.editorSessions_.contains("child"), "close descendant sessions only");
    require(read(external) == "Updated", "save external document alongside deletion");
    Notebook todos{"ui-todo", "Todos", "todo", {}, {}, temp.filePath("ui-todo")};
    for (int i = 0; i < 47; ++i) todos.notes.append({"todo-" + QString::number(i), {}, QString::number(i), "Body", false, {{"completed", i % 2 == 0}, {"category", i % 3 == 0 ? "A" : "B"}}});
    window.store_.notebooks.append(todos);
    window.currentNotebookId_ = todos.id;
    window.refreshNotebooks();
    require(window.save(), "save UI todos");
    window.todoWorkspace_->todoCategoryFilter_ = "A";
    window.todoWorkspace_->todoStatus_->setCurrentIndex(1);
    window.search_->setText("no visible matches");
    QTimer dialogs;
    bool blockedWarning = false;
    QObject::connect(&dialogs, &QTimer::timeout, [&] {
        if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
            if (box->standardButtons().testFlag(QMessageBox::Yes)) box->button(QMessageBox::Yes)->click();
            else {
                blockedWarning = true;
                box->accept();
            }
        }
    });
    dialogs.start(10);
    window.todoWorkspace_->clearCompletedTodos();
    require(window.store_.notebook(todos.id)->notes.size() == 23, "UI batch ignores filter/search/page");
    window.todoWorkspace_->deleteTodo("todo-1");
    require(!window.store_.note("todo-1"), "UI individual todo deletion");
    window.storageReady_ = false;
    window.refreshNotebooks();
    const auto guardedPath = todos.path + '/' + window.store_.note("todo-3")->path;
    const auto guardedBody = read(guardedPath);
    window.todoWorkspace_->deleteTodo("todo-3");
    require(blockedWarning && window.store_.note("todo-3") && read(guardedPath) == guardedBody, "failed-load guard prevents destructive operation");
    qInfo() << "PASS: folder/ebook/site/todo deletion, descendants/assets, batch, save/cleanup failures, UI sessions";
}
