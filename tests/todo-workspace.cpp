#include "core/NotebookStore.h"
#include "ui/TodoWorkspace.h"
#include <QtWidgets>
#define private public
#include "ui/MainWindow.h"
#undef private

namespace {
void require(bool condition, const QString &message) {
    if (!condition) qFatal("%s", qPrintable(message));
}
void events() {
    QEventLoop loop;
    QTimer::singleShot(30, &loop, &QEventLoop::quit);
    loop.exec();
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
    store.notebooks = {{"normal", "Normal", "normal", {{"markdown", {}, "Markdown", "# Heading", false}}, {}, temp.filePath("normal")},
                       {"todo", "Todo", "todo", {}, {}, temp.filePath("todo")},
                       {"second", "Second", "todo", {{"only", {}, "Only", "Body", false}}, {}, temp.filePath("second")}};
    for (int i = 0; i < 45; ++i) {
        store.notebooks[1].notes.append({QString::number(i), {}, "Task " + QString::number(i), "Body", false, {{"priority", i % 3}, {"category", i % 2 ? "A" : "B"}, {"created", QDateTime(QDate(2026, 1, 1).addDays(i), QTime(12, 0)).toString(Qt::ISODate)}, {"due", QDate(2026, 12, 31).addDays(-i).toString(Qt::ISODate)}}});
    }
    QString error;
    require(store.save(&error), error);
    QSettings().setValue("session/notebookId", "normal");
    QSettings().setValue("session/noteId", "markdown");
    QSettings().setValue("session/openNotes", QStringList{"markdown"});
    MainWindow window;
    window.resize(1200, 800);
    window.show();
    events();
    window.notebooks_->setCurrentIndex(window.notebooks_->findData("todo"));
    auto *workspace = window.todoWorkspace_;
    auto *list = workspace->findChild<QTreeWidget *>("todoList");
    auto *sort = workspace->findChild<QComboBox *>("todoSort");
    auto *status = workspace->categoryPanel()->findChild<QComboBox *>("todoStatus");
    auto *categories = workspace->categoryPanel()->findChild<QTreeWidget *>();
    auto *details = workspace->detailsPanel();
    require(workspace->isVisible() && workspace->categoryPanel()->isVisible() && details->isVisible(), "all todo panels visible");
    require(window.tabs_->count() == 1 && list->topLevelItemCount() == 20, "Markdown session retained and todos paginated");
    require(list->topLevelItem(0)->data(0, Qt::UserRole) == "2", "priority sort");
    sort->setCurrentIndex(1);
    require(list->topLevelItem(0)->data(0, Qt::UserRole) == "44", "created sort newest first");
    sort->setCurrentIndex(2);
    require(list->topLevelItem(0)->data(0, Qt::UserRole) == "44", "due sort earliest first");
    workspace->findChild<QPushButton *>("todoNextPage")->click();
    require(list->topLevelItem(0)->data(0, Qt::UserRole) == "24", "next page");
    workspace->findChild<QPushButton *>("todoPreviousPage")->click();
    require(list->topLevelItem(0)->data(0, Qt::UserRole) == "44", "previous page");
    window.search_->setText("Task 17");
    require(list->topLevelItemCount() == 1 && list->currentItem()->data(0, Qt::UserRole) == "17", "global search forwarded to todo workspace");
    auto *title = details->findChild<QLineEdit *>("todoTitle");
    title->setText("Updated Task 17");
    emit title->textEdited(title->text());
    details->findChild<QPlainTextEdit *>("todoContent")->setPlainText("Updated body");
    require(window.store_.note("17")->title == "Updated Task 17" && window.store_.note("17")->content == "Updated body" && window.saveTimer_->isActive(), "title/body edits schedule save");
    auto *category = details->findChild<QComboBox *>("todoCategory");
    category->setCurrentIndex(category->findData("B"));
    require(window.store_.note("17")->metadata["category"] == "B" && window.store_.note("17")->path.startsWith("B/"), "category edit saves and moves file");
    details->findChild<QComboBox *>("todoPriority")->setCurrentIndex(2);
    details->findChild<QDateEdit *>("todoDue")->setDate(QDate(2026, 11, 1));
    require(window.store_.note("17")->metadata["priority"].toInt() == 2 && window.store_.note("17")->metadata["due"] == "2026-11-01", "priority/due edit");
    auto *row = list->currentItem();
    row->setCheckState(0, Qt::Checked);
    require(list->currentItem() == row && window.store_.note("17")->metadata["completed"].toBool(), "completion keeps row alive during item event");
    events();
    require(list->topLevelItemCount() == 0, "completed row removed after event returns");
    status->setCurrentIndex(1);
    require(list->topLevelItemCount() == 1, "completed filter");
    details->findChild<QCheckBox *>("todoCompleted")->setChecked(false);
    require(list->topLevelItemCount() == 0 && !window.store_.note("17")->metadata["completed"].toBool(), "detail completion toggle");
    status->setCurrentIndex(0);
    window.search_->clear();
    for (int i = 0; i < categories->topLevelItemCount(); ++i) {
        if (categories->topLevelItem(i)->data(0, Qt::UserRole) == "A") categories->setCurrentItem(categories->topLevelItem(i));
    }
    for (int i = 0; i < list->topLevelItemCount(); ++i) require(window.store_.note(list->topLevelItem(i)->data(0, Qt::UserRole).toString())->metadata["category"] == "A", "category filter");
    window.notebooks_->setCurrentIndex(window.notebooks_->findData("second"));
    require(list->topLevelItemCount() == 1 && list->currentItem()->data(0, Qt::UserRole) == "only" && status->currentIndex() == 0, "switch notebook resets category/status/page");
    window.notebooks_->setCurrentIndex(window.notebooks_->findData("normal"));
    events();
    require(window.currentNoteId() == "markdown" && window.tabs_->count() == 1 && !workspace->categoryPanel()->isVisible() && !details->isVisible(), "return to Markdown session");
    Notebook reloaded;
    require(NotebookStore::readNotebook(temp.filePath("todo"), reloaded, &error), error);
    for (const auto &note: reloaded.notes)
        if (note.id == "17") require(note.title == "Updated Task 17" && note.content == "Updated body", "edits persisted");
    qInfo() << "PASS: todo editing, sorting, pagination, search, category, deferred completion, notebook switching and Markdown session";
}
