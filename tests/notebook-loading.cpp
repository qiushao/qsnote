#include "core/NotebookStore.h"
#include <QtWidgets>
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
}// namespace

int main(int argc, char **argv) {
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QApplication app(argc, argv);
    QTemporaryDir temp;
    require(temp.isValid(), "temporary directory");
    qputenv("XDG_DATA_HOME", temp.path().toUtf8());
    QCoreApplication::setOrganizationName("qsnote-regression");
    QCoreApplication::setApplicationName("notebook-loading");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temp.path());
    QSettings().setValue("General/checkUpdatesOnStartup", false);
    QSettings().setValue("General/minimizeToTray", false);
    QString error;
    for (const auto &type: {QString("normal"), QString("site"), QString("ebook"), QString("todo")}) {
        NotebookStore store;
        store.notebooks.append({type, type, type, {{"text", {}, "Text", "Original body\n", false}, {"empty", {}, "Empty", {}, false}}, {}, temp.filePath(type)});
        require(store.save(&error), error);
        Notebook loaded;
        require(NotebookStore::readNotebook(store.notebooks.first().path, loaded, &error), error);
        require(loaded.notes.first().content == "Original body\n", "body round trip: " + type);
        require(loaded.notes.last().content.isEmpty(), "empty body: " + type);
        store.notebooks = {loaded};
        store.notebooks.first().notes.first().content = "Updated body\n";
        store.notebooks.first().notes.first().contentDirty = true;
        require(store.save(&error), error);
        require(NotebookStore::readNotebook(loaded.path, loaded, &error), error);
        require(loaded.notes.first().content == "Updated body\n", "save round trip: " + type);

        const auto bodyPath = QDir(loaded.path).filePath(loaded.notes.first().path);
        const auto original = read(bodyPath);
        const auto indexPath = QDir(loaded.path).filePath(NotebookStore::indexFileName());
        const auto index = read(indexPath);
        const auto registry = read(NotebookStore::storagePath());
        require(QFile::setPermissions(bodyPath, {}), "remove body read permission");
        error.clear();
        Notebook unreadable;
        require(!NotebookStore::readNotebook(loaded.path, unreadable, &error), "unreadable body must fail: " + type);
        require(error.contains(bodyPath), "error must identify body: " + error);
        // Exercise the actual MainWindow save guard, including the site's unconditional body writes.
        {
            MainWindow window;
            require(!window.storageReady_, "failed load must disable saving");
            QTimer dismiss;
            QObject::connect(&dismiss, &QTimer::timeout, [] {
                for (auto *widget: QApplication::topLevelWidgets()) {
                    if (auto *box = qobject_cast<QMessageBox *>(widget)) box->accept();
                }
            });
            dismiss.start(10);
            require(!window.save(), "save after failed load must fail");
        }
        require(QFile::setPermissions(bodyPath, QFileDevice::ReadOwner | QFileDevice::WriteOwner), "restore permissions");
        require(read(bodyPath) == original && read(indexPath) == index && read(NotebookStore::storagePath()) == registry,
                "failed load/save must leave disk unchanged");

        // On Linux opening a directory for reading succeeds but readAll reports an error.
        require(QFile::rename(bodyPath, bodyPath + ".backup"), "move body aside");
        require(QDir().mkdir(bodyPath), "replace body with directory");
        error.clear();
        require(!NotebookStore::readNotebook(loaded.path, unreadable, &error) && error.contains(bodyPath), "body read error must fail");
        require(QDir().rmdir(bodyPath), "remove directory fixture");
        require(!NotebookStore::readNotebook(loaded.path, unreadable, &error), "missing body must fail");
        require(QFile::rename(bodyPath + ".backup", bodyPath), "restore body");
    }
    qInfo() << "PASS: normal/site/ebook/todo round trips, empty/unreadable/missing/read-error bodies, failed-load save protection";
}
