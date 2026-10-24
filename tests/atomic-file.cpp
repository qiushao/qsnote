#include "core/AtomicFile.h"
#include "core/NotebookStore.h"
#include "core/StaticSite.h"
#include <QtCore>
void require(bool condition, const QString &message) {
    if (!condition) qFatal("%s", qPrintable(message));
}
QByteArray read(const QString &path) {
    QFile file(path);
    require(file.open(QIODevice::ReadOnly), file.errorString());
    return file.readAll();
}
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir temp;
    require(temp.isValid(), "temporary directory");
    qputenv("XDG_DATA_HOME", temp.path().toUtf8());
    QString error;
    const auto path = temp.filePath("nested/file.md");
    require(AtomicFile::write(path, "first", &error), error);
    QFile file(path);
    require(file.open(QIODevice::ReadWrite), file.errorString());
    const auto stamp = QDateTime::fromSecsSinceEpoch(1000000000);
    require(file.setFileTime(stamp, QFileDevice::FileModificationTime), "set known timestamp");
    file.close();
    require(AtomicFile::write(path, "first", &error), error);
    require(QFileInfo(path).lastModified() == stamp, "identical content must retain timestamp");
    require(AtomicFile::write(path, "changed", &error), error);
    require(read(path) == "changed", "changed bytes must be saved");
    require(!AtomicFile::write(path + "/child", "text", &error) && !error.isEmpty(), "parent creation failure");
    error.clear();
    require(!AtomicFile::write(temp.path(), {}, &error) && !error.isEmpty(), "directory read must not count as equal empty file");
    const auto parent = QFileInfo(path).absolutePath();
    require(QFile::setPermissions(parent, QFileDevice::ReadOwner | QFileDevice::ExeOwner), "lock directory");
    error.clear();
    require(!AtomicFile::write(path, "replacement", &error) && !error.isEmpty(), "failed atomic replacement must report error");
    require(read(path) == "changed", "failed write must retain original bytes");
    require(QFile::setPermissions(parent, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner), "unlock directory");
    NotebookStore store;
    store.notebooks.append({"site", "Site", "site", {{"post", {}, "Post", "Body\n", false}}, {}, temp.filePath("site")});
    require(store.save(&error), error);
    const auto &book = store.notebooks.first();
    const auto index = read(book.path + '/' + NotebookStore::indexFileName());
    require(!index.isEmpty() && read(book.path + '/' + book.notes.first().path).endsWith("Body\n"), "site index and post saved");
    for (const auto *page: {"categories", "tags", "links", "about"}) require(!read(book.path + "/source/" + page + "/index.md").isEmpty(), "default page saved");
    Notebook restored;
    require(NotebookStore::readNotebook(book.path, restored, &error) && restored.notes.first().content == "Body\n", "notebook round trip");
    qInfo() << "PASS: parent directories, equal-content timestamp, changed bytes, write failures, site index/post/default pages";
}
