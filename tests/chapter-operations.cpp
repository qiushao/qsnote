#include "core/ChapterOperations.h"
#include "core/EbookExporter.h"
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
void verifyExport(const Notebook &book, const QString &directory, const QStringList &titles) {
    QString error;
    const auto path = EbookExporter::exportBook(book, {}, EbookExporter::Format::Html, directory, &error);
    require(!path.isEmpty(), error);
    const auto html = QString::fromUtf8(read(path));
    qsizetype previous = -1;
    for (const auto &title: titles) {
        const auto index = html.indexOf("<h1>" + title + "</h1>");
        require(index > previous, "export order/numbering: " + title);
        previous = index;
    }
    require(html.count("src=\"data:image/png;base64,") == 2, "export retains parent and child images");
}
void menuAction(MainWindow &window, const QString &id, const QString &title, bool enabled, bool trigger) {
    QTreeWidgetItemIterator it(window.tree_);
    while (*it && (*it)->data(0, Qt::UserRole).toString() != id) ++it;
    require(*it, "tree chapter");
    window.tree_->scrollToItem(*it);
    const auto position = window.tree_->visualItemRect(*it).center();
    QTimer::singleShot(0, [&] {
        auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
        require(menu, "chapter menu");
        QAction *target = nullptr;
        for (auto *action: menu->actions())
            if (action->text() == title) target = action;
        require(target && target->isEnabled() == enabled, "menu availability: " + title);
        if (trigger) target->trigger();
        menu->close();
    });
    window.showTreeMenu(position);
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
    NotebookStore store;
    store.notebooks = {{"book", "Book", "ebook", {{"a", {}, "A", "A body", false}, {"a1", "a", "A1", "A1 body", false}, {"b", {}, "B", "![Parent](resources/images/parent.png)", false}, {"b1", "b", "B1", "![Child](resources/images/child.png)", false}, {"c", {}, "C", "C body", false}}, {{"autoNumberChapters", true}}, temp.filePath("book")}};
    auto &book = store.notebooks.first();
    QString error;
    require(store.save(&error), error);
    QHash<QString, QByteArray> images;
    for (const auto &id: {QString("b"), QString("b1")}) {
        const auto directory = QFileInfo(book.path + '/' + store.note(id)->path).absolutePath() + "/resources/images";
        require(QDir().mkpath(directory), "image directory");
        const auto path = directory + (id == "b" ? "/parent.png" : "/child.png");
        QImage image(8, 8, QImage::Format_RGB32);
        image.fill(id == "b" ? Qt::red : Qt::blue);
        require(image.save(path), "image fixture");
        images[id] = read(path);
    }
    using Move = ChapterOperations::Move;
    for (const auto movement: {Move::Up, Move::Indent, Move::Outdent}) {
        require(!ChapterOperations::canMove(book, "a", movement) && !ChapterOperations::move(book, "a", movement), "first/root boundary");
    }
    require(!ChapterOperations::move(book, "c", Move::Down), "last sibling boundary");
    require(ChapterOperations::move(book, "b", Move::Up) && book.notes.first().id == "b", "move across interleaved child entries");
    require(ChapterOperations::move(book, "b", Move::Down) && book.notes.first().id == "a", "move down restores root order");
    require(ChapterOperations::move(book, "b", Move::Indent) && store.save(&error), error);
    require(store.note("b")->parentId == "a" && store.note("b")->path == "A/B.md" && store.note("b1")->path == "A/B/B1.md", "indent parent and descendant paths");
    require(NotebookStore::chapterTitles(book)["b1"] == "1.2.1 B1", "indent automatic numbering");
    for (const auto &id: {QString("b"), QString("b1")}) {
        const auto directory = QFileInfo(book.path + '/' + store.note(id)->path).absolutePath();
        require(read(directory + (id == "b" ? "/resources/images/parent.png" : "/resources/images/child.png")) == images[id], "indent image retained");
    }
    verifyExport(book, temp.path(), {"1 A", "1.1 A1", "1.2 B", "1.2.1 B1", "2 C"});
    require(ChapterOperations::move(book, "b", Move::Up) && store.save(&error), error);
    require(NotebookStore::chapterTitles(book)["b"] == "1.1 B", "nested sibling move");
    require(ChapterOperations::move(book, "b", Move::Outdent) && store.save(&error), error);
    require(store.note("b")->parentId.isEmpty() && store.note("b")->path == "B.md" && store.note("b1")->path == "B/B1.md", "outdent paths");
    verifyExport(book, temp.path(), {"1 A", "1.1 A1", "2 B", "2.1 B1", "3 C"});
    Notebook restored;
    require(NotebookStore::readNotebook(book.path, restored, &error), error);
    require(NotebookStore::chapterTitles(restored) == NotebookStore::chapterTitles(book), "reload numbering and ordering");
    auto unusual = book;
    for (int i = 0; i < unusual.notes.size(); ++i)
        if (unusual.notes[i].id == "b1") {
            unusual.notes.move(i, 0);
            break;
        }
    require(ChapterOperations::move(unusual, "b1", Move::Outdent), "outdent a child stored before parent");
    QStringList roots;
    for (const auto &note: unusual.notes)
        if (note.parentId.isEmpty()) roots.append(note.id);
    require(roots == QStringList{"a", "b", "b1", "c"}, "outdent inserts immediately after parent");

    QSettings().setValue("session/notebookId", "book");
    MainWindow window;
    window.show();
    menuAction(window, "a", "上移", false, false);
    menuAction(window, "c", "下移", false, false);
    menuAction(window, "a", "提升层级", false, false);
    menuAction(window, "b", "缩进为子章节", true, true);
    require(window.store_.note("b")->parentId == "a" && window.store_.note("b1")->path == "A/B/B1.md", "UI move delegates and saves");
    qInfo() << "PASS: chapter boundaries, sibling order, indent/outdent paths, images, numbering, export order and UI menu";
}
