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
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc, argv);
    QTemporaryDir temp;
    require(temp.isValid(), "temporary directory");
    qputenv("XDG_DATA_HOME", temp.path().toUtf8());
    qputenv("XDG_CACHE_HOME", temp.path().toUtf8());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temp.path());
    QSettings().setValue("General/checkUpdatesOnStartup", false);
    NotebookStore store;
    store.notebooks = {{"site", "Site", "site", {}, {}, temp.filePath("site")}};
    QString error;
    require(store.save(&error), error);
    QSettings().setValue("session/notebookId", "site");
    MainWindow window;
    window.resize(1200, 800);
    window.show();
    const auto indexPath = store.notebooks.first().path + "/_index.json";
    const auto originalIndex = read(indexPath);
    QImage avatar(12, 12, QImage::Format_RGB32);
    avatar.fill(Qt::blue);
    const auto avatarPath = temp.filePath("avatar.png");
    require(avatar.save(avatarPath), "avatar fixture");
    QTimer::singleShot(0, [&] {
        auto *dialog = QApplication::activeModalWidget();
        require(dialog && dialog->objectName() == "siteConfigDialog", "site config dialog");
        auto &config = window.store_.notebooks.first().config;
        auto *title = dialog->findChild<QLineEdit *>("config_title");
        title->setText("Site title");
        emit title->textEdited(title->text());
        require(config["title"] == "Site title" && window.saveTimer_->isActive(), "edits apply in memory and schedule save");
        require(read(indexPath) == originalIndex, "text edit does not immediately save");
        for (auto *line: dialog->findChildren<QLineEdit *>()) {
            if (!line->objectName().startsWith("config_")) continue;
            const auto key = line->objectName().mid(7);
            const auto value = key == "domain" ? "https://example.com/blog/" : key + " value";
            line->setText(value);
            emit line->textEdited(value);
            require(config[key] == value, "field updates: " + key);
        }
        auto *comments = dialog->findChild<QComboBox *>("config_commentPlatform");
        for (const auto &platform: {QString("gitalk"), QString("disqus"), QString("valine"), QString("none")}) {
            comments->setCurrentIndex(comments->findData(platform));
            require(dialog->findChild<QLineEdit *>("config_clientSecret")->parentWidget()->isHidden() == (platform != "gitalk"), "comment field linkage");
        }
        require(dialog->findChild<QLineEdit *>("config_clientSecret")->echoMode() == QLineEdit::Password, "comment secret remains masked");
        auto *theme = dialog->findChild<QComboBox *>("config_theme");
        theme->setCurrentIndex(theme->findData("custom"));
        require(!dialog->findChild<QLineEdit *>("config_themeDirectory")->parentWidget()->isHidden(), "custom theme fields");
        auto *publish = dialog->findChild<QComboBox *>("config_publishMethod");
        for (const auto &method: {QString("github"), QString("scp")}) {
            publish->setCurrentIndex(publish->findData(method));
            require(dialog->findChild<QLineEdit *>("config_sshHost")->parentWidget()->isHidden() == (method != "scp"), "publish field linkage");
        }
        for (auto *code: dialog->findChildren<QPlainTextEdit *>()) {
            if (code->objectName().startsWith("config_")) code->setPlainText("<script>/* " + code->objectName() + " */</script>");
        }
        dialog->findChild<QPushButton *>("addLink")->click();
        auto *name = dialog->findChild<QLineEdit *>("name");
        auto *url = dialog->findChild<QLineEdit *>("url");
        name->setText("Friend");
        emit name->textEdited(name->text());
        url->setText("https://example.org/");
        emit url->textEdited(url->text());
        require(config["links"].toArray().first().toObject()["name"] == "Friend", "friend link edits");
        QTimer::singleShot(0, [&] {
            auto *picker = qobject_cast<QFileDialog *>(QApplication::activeModalWidget());
            require(picker, "avatar picker");
            picker->selectFile(avatarPath);
            static_cast<QDialog *>(picker)->accept();
        });
        dialog->findChild<QPushButton *>("chooseSiteAvatar")->click();
        require(config["avatar"] == "images/qsnote-avatar.png" && !QImage(store.notebooks.first().path + "/source/images/qsnote-avatar.png").isNull(), "avatar import");
        require(QMetaObject::invokeMethod(window.saveTimer_, "timeout", Qt::DirectConnection), "invoke pending save");
        require(read(indexPath).contains("Friend"), "delayed save works while dialog is open");
        title->setText("Saved on close");
        emit title->textEdited(title->text());
        dialog->findChild<QPushButton *>("closeSiteConfig")->click();
    });
    window.showSiteConfig();
    const auto expected = window.store_.notebooks.first().config;
    Notebook restored;
    require(NotebookStore::readNotebook(store.notebooks.first().path, restored, &error) && restored.config == expected, "close persists all config");
    QTimer::singleShot(0, [&] {
        auto *dialog = QApplication::activeModalWidget();
        for (auto *line: dialog->findChildren<QLineEdit *>()) {
            if (line->objectName().startsWith("config_")) require(line->text() == expected[line->objectName().mid(7)].toString(), "restore line edit");
        }
        for (auto *select: dialog->findChildren<QComboBox *>()) {
            if (select->objectName().startsWith("config_")) require(select->currentData().toString() == expected[select->objectName().mid(7)].toString(), "restore choice");
        }
        for (auto *code: dialog->findChildren<QPlainTextEdit *>()) {
            if (code->objectName().startsWith("config_")) require(code->toPlainText() == expected[code->objectName().mid(7)].toString(), "restore custom code");
        }
        require(dialog->findChild<QLineEdit *>("name")->text() == "Friend", "restore links");
        dialog->findChild<QPushButton *>("removeLink")->click();
        require(window.store_.notebooks.first().config["links"].toArray().isEmpty(), "remove link immediately");
        static_cast<QDialog *>(dialog)->reject();
    });
    window.showSiteConfig();
    require(NotebookStore::readNotebook(store.notebooks.first().path, restored, &error) && restored.config["links"].toArray().isEmpty(), "reject closes and saves immediate edits");
    qInfo() << "PASS: all site fields, avatar, linkage, links, custom code, immediate edits/delayed save/close save and restoration";
}
