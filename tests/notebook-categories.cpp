#include "core/NotebookCategories.h"
#include <QtWidgets>
#define private public
#include "ui/MainWindow.h"
#include "ui/TodoWorkspace.h"
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
void answer(const QString &text) {
    QTimer::singleShot(0, [text] {
        auto *input = qobject_cast<QInputDialog *>(QApplication::activeModalWidget());
        require(input, "category input dialog");
        input->setTextValue(text);
        input->accept();
    });
}
void confirm() {
    QTimer::singleShot(0, [] {
        auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
        require(box, "category deletion confirmation");
        box->button(QMessageBox::Yes)->click();
    });
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
    for (const auto &type: {QString("todo"), QString("site")}) {
        NotebookStore store;
        store.notebooks.append({type, type, type, {{"move", {}, "Move", "![Image](resources/images/pic.png)\n", false, {{"category", "implicit"}}}, {"keep", {}, "Keep", "Keep", false, {{"category", "other"}}}}, {{"categories", QJsonArray{"explicit", "implicit", "explicit"}}}, temp.filePath(type)});
        auto &book = store.notebooks.first();
        const auto fallback = type == "site" ? QString("default") : QString("默认");
        const auto names = type == "site" ? NotebookCategories::site(book) : NotebookCategories::todo(book);
        require(names == QStringList{fallback, "explicit", "implicit", "other"}, "collect configured and used categories in order");
        require(!NotebookCategories::create(book, "other", &error), "reject category used only by a note");
        require(!NotebookCategories::rename(book, "implicit", "explicit", &error), "reject duplicate rename");
        require(!NotebookCategories::rename(book, fallback, "changed", &error) && !NotebookCategories::remove(book, fallback, &error), "protect default");
        require(!NotebookCategories::create(book, "   ", &error), "reject empty name");
        require(!NotebookCategories::rename(book, "missing", "changed", &error) && !NotebookCategories::remove(book, "missing", &error), "reject missing category");
        if (type == "todo") require(!NotebookCategories::create(book, "全部", &error), "reserve All filter");
        require(NotebookCategories::create(book, " new ", &error), "create trimmed category");
        require(store.save(&error), error);
        const auto originalDirectory = QFileInfo(book.path + '/' + store.note("move")->path).absolutePath();
        require(QDir().mkpath(originalDirectory + "/resources/images"), "image directory");
        QImage image(8, 8, QImage::Format_RGB32);
        image.fill(Qt::red);
        require(image.save(originalDirectory + "/resources/images/pic.png"), "image fixture");
        const auto imageBytes = read(originalDirectory + "/resources/images/pic.png");
        const auto keepPath = store.note("keep")->path;
        require(NotebookCategories::rename(book, "implicit", "renamed", &error) && store.save(&error), error);
        require(store.note("move")->metadata["category"] == "renamed" && store.note("move")->path.contains("renamed/"), "rename metadata and path");
        auto directory = QFileInfo(book.path + '/' + store.note("move")->path).absolutePath();
        require(read(directory + "/resources/images/pic.png") == imageBytes, "rename retains referenced image");
        require(NotebookCategories::remove(book, "renamed", &error) && store.save(&error), error);
        require(store.note("move")->metadata["category"] == fallback && store.note("move")->path.contains(fallback + '/'), "delete migrates to default");
        directory = QFileInfo(book.path + '/' + store.note("move")->path).absolutePath();
        require(read(directory + "/resources/images/pic.png") == imageBytes, "delete retains referenced image");
        require(store.note("keep")->path == keepPath && store.note("keep")->metadata["category"] == "other", "unrelated category unchanged");
        Notebook restored;
        require(NotebookStore::readNotebook(book.path, restored, &error), error);
        require(restored.notes.first().content == "![Image](resources/images/pic.png)\n" && restored.notes.first().metadata["category"] == fallback, "reload metadata/body");
    }
    QTranslator english;
    require(english.load(":/translations/qsnote_en_US.qm"), "load English translations");
    app.installTranslator(&english);
    Notebook translated;
    translated.type = "todo";
    require(!NotebookCategories::create(translated, QCoreApplication::translate("MainWindow", "全部"), &error), "reserve translated All");
    require(!NotebookCategories::create(translated, QCoreApplication::translate("MainWindow", "默认"), &error), "reserve translated Default");
    app.removeTranslator(&english);

    NotebookStore store;
    store.notebooks = {{"todo", "Todo", "todo", {{"task", {}, "Task", "Body", false, {{"category", "Old"}}}}, {}, temp.filePath("ui-todo")},
                       {"site", "Site", "site", {{"post", {}, "Post", "Body", false, {{"category", "Old"}}}}, {}, temp.filePath("ui-site")}};
    require(store.save(&error), error);
    QSettings().setValue("session/notebookId", "todo");
    MainWindow window;
    window.show();
    answer("New");
    window.todoWorkspace_->createTodoCategory();
    require(window.todoWorkspace_->todoCategoryFilter_ == "New" && window.todoWorkspace_->todoCategory_->findData("New") >= 0, "new todo category selected");
    window.todoWorkspace_->todoCategoryFilter_ = "Old";
    window.todoWorkspace_->refreshTodoCategories();
    answer("Renamed");
    window.todoWorkspace_->renameTodoCategory("Old");
    require(window.todoWorkspace_->todoCategoryFilter_ == "Renamed" && window.store_.note("task")->metadata["category"] == "Renamed", "todo rename preserves filter");
    confirm();
    window.todoWorkspace_->deleteTodoCategory("Renamed");
    require(window.todoWorkspace_->todoCategoryFilter_.isEmpty() && window.store_.note("task")->metadata["category"] == "默认", "todo delete resets filter");

    window.currentNotebookId_ = "site";
    window.refreshNotebooks();
    QTimer::singleShot(0, [&] {
        auto *dialog = QApplication::activeModalWidget();
        require(dialog && dialog->objectName() == "siteConfigDialog", "site config dialog");
        auto *list = dialog->findChild<QListWidget *>("siteCategories");
        auto *rename = dialog->findChild<QPushButton *>("renameSiteCategory");
        auto *remove = dialog->findChild<QPushButton *>("deleteSiteCategory");
        require(!rename->isEnabled() && !remove->isEnabled(), "default category buttons disabled");
        answer("New");
        dialog->findChild<QPushButton *>("addSiteCategory")->click();
        require(list->currentItem()->text() == "New", "site create selects category");
        list->setCurrentItem(list->findItems("Old", Qt::MatchExactly).first());
        answer("Renamed");
        rename->click();
        require(list->currentItem()->text() == "Renamed" && window.store_.note("post")->metadata["category"] == "Renamed", "site rename migrates and selects");
        confirm();
        remove->click();
        require(list->currentItem()->text() == "default" && window.store_.note("post")->metadata["category"] == "default", "site delete migrates and selects default");
        dialog->findChild<QPushButton *>("closeSiteConfig")->click();
    });
    window.showSiteConfig();
    qInfo() << "PASS: category collection/validation/defaults, translations, path/image migration, todo/site UI selection";
}
