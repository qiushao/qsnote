#include "core/NotebookStore.h"
#include <QtWidgets>
#define private public
#include "ui/MainWindow.h"
#include "ui/PostPropertiesDialog.h"
#undef private

namespace {
void require(bool condition, const QString &message) {
    if (!condition) qFatal("%s", qPrintable(message));
}
void fill(PostPropertiesDialog &dialog, const QString &title) {
    dialog.title_->setText(" " + title + " ");
    dialog.category_->setCurrentText("Category");
    dialog.tags_->setText(" tag1,tag2 ");
    dialog.date_->setDate(QDate(2026, 10, 24));
    dialog.slug_->setText(" custom-slug ");
    dialog.draft_->setChecked(true);
    if (dialog.keywords_) dialog.keywords_->setText(" keywords ");
}
void editPost(MainWindow &window, const QString &id, bool accept) {
    QTreeWidgetItemIterator it(window.tree_);
    while (*it && (*it)->data(0, Qt::UserRole).toString() != id) ++it;
    require(*it, "post tree item");
    const auto position = window.tree_->visualItemRect(*it).center();
    QTimer::singleShot(0, [&] {
        auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
        require(menu, "post menu");
        QAction *properties = nullptr;
        for (auto *action: menu->actions())
            if (action->text() == "属性更改") properties = action;
        require(properties, "properties action");
        QTimer::singleShot(0, [&] {
            auto *dialog = dynamic_cast<PostPropertiesDialog *>(QApplication::activeModalWidget());
            require(dialog && dialog->keywords_, "editing form contains keywords");
            fill(*dialog, "Edited");
            if (accept) dialog->accept();
            else
                dialog->reject();
        });
        properties->trigger();
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
    store.notebooks = {{"site", "Site", "site", {}, {{"categories", QJsonArray{"Category"}}}, temp.filePath("site")}};
    QString error;
    require(store.save(&error), error);
    QSettings().setValue("session/notebookId", "site");
    MainWindow window;
    window.show();
    for (bool accept: {false, true}) {
        QTimer::singleShot(0, [&] {
            auto *dialog = dynamic_cast<PostPropertiesDialog *>(QApplication::activeModalWidget());
            require(dialog, "new post uses shared dialog");
            require(dialog->title().isEmpty() && dialog->category_->currentText() == "Category", "new title/category defaults");
            require(dialog->date_->date() == QDate::currentDate() && dialog->slug_->text().startsWith("post-"), "new date/slug defaults");
            require(!dialog->keywords_ && !dialog->draft_->isChecked() && dialog->tags_->text().isEmpty(), "create excludes keywords and preserves defaults");
            fill(*dialog, "Created");
            if (accept) dialog->accept();
            else
                dialog->reject();
        });
        window.createNote({}, false, "Category");
        require(window.store_.notebooks.first().notes.size() == (accept ? 1 : 0), "cancel/create mutation boundary");
    }
    auto *note = &window.store_.notebooks.first().notes.first();
    const auto id = note->id;
    const auto metadata = note->metadata;
    require(note->title == "Created" && metadata["tags"] == "tag1,tag2" && metadata["slug"] == "custom-slug" && metadata["draft"].toBool(), "created form values");
    require(!metadata.contains("keywords"), "create does not introduce SEO keywords");
    note->metadata["custom"] = "preserved";
    require(window.save(), "save before edit");
    editPost(window, id, false);
    require(note->title == "Created" && !note->metadata.contains("keywords"), "cancel editing preserves note");
    editPost(window, id, true);
    require(note->title == "Edited" && note->metadata["keywords"] == "keywords" && note->metadata["custom"] == "preserved", "editing updates known fields only");
    Notebook reloaded;
    require(NotebookStore::readNotebook(window.store_.notebooks.first().path, reloaded, &error), error);
    require(reloaded.notes.first().title == "Edited" && reloaded.notes.first().metadata == note->metadata, "reopen saved metadata");
    PostPropertiesDialog reopened(note->title, note->metadata, {"default", "Category"}, true);
    require(reopened.title() == note->title && reopened.metadata() == note->metadata, "form restores metadata");
    QTranslator english;
    require(english.load(":/translations/qsnote_en_US.qm"), "English translation");
    app.installTranslator(&english);
    PostPropertiesDialog translated({}, {}, {"default"}, true);
    require(translated.windowTitle() == QCoreApplication::translate("MainWindow", "文章属性") && translated.windowTitle() != "文章属性", "preserve translation context");
    qInfo() << "PASS: shared create/edit form, defaults, cancel, metadata round trip, SEO difference and English context";
}
