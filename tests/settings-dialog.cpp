#include "ui/MarkdownEditor.h"
#include "ui/SettingsDialog.h"
#include <QtWidgets>
#include <vtextedit/texteditorconfig.h>
#include <vtextedit/vtextedit.h>
#define private public
#include "ui/MainWindow.h"
#undef private

namespace {
void require(bool condition, const QString &message) {
    if (!condition) qFatal("%s", qPrintable(message));
}
QVariantMap values() {
    QSettings settings;
    QVariantMap result;
    for (const auto &key: settings.allKeys()) {
        if (key.startsWith("General/") || key.startsWith("editor/")) result[key] = settings.value(key);
    }
    return result;
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
    MainWindow window;
    window.resize(1200, 800);
    window.show();
    require(!window.markdownEditor_, "empty workspace");
    QString selectedFamily;
    for (bool save: {false, true}) {
        const auto before = values();
        QTimer::singleShot(0, [&] {
            auto *dialog = qobject_cast<SettingsDialog *>(QApplication::activeModalWidget());
            require(dialog, "settings component");
            require(dialog->findChild<QTabWidget *>("editorSettingsTabs"), "editor settings available without an open note");
            dialog->findChild<QComboBox *>("uiLanguage")->setCurrentIndex(2);
            auto *familyInput = dialog->findChild<QFontComboBox *>("uiFontFamily");
            familyInput->setCurrentFont(QFont(QFontDatabase::families().first()));
            selectedFamily = familyInput->currentFont().family();
            dialog->findChild<QSpinBox *>("uiFontSize")->setValue(18);
            dialog->findChild<QCheckBox *>("minimizeToTray")->setChecked(true);
            dialog->findChild<QCheckBox *>("checkUpdatesOnStartup")->setChecked(false);
            dialog->findChild<QSpinBox *>("fontSize")->setValue(19);
            dialog->findChild<QSpinBox *>("tabWidth")->setValue(7);
            require(values() == before, "form edits do not apply before saving");
            auto *buttons = dialog->findChild<QDialogButtonBox *>();
            buttons->button(save ? QDialogButtonBox::Save : QDialogButtonBox::Cancel)->click();
        });
        window.showSettings();
        if (!save) require(values() == before, "cancel leaves all settings unchanged");
    }
    QSettings settings;
    require(settings.value("General/language") == "en_US" && settings.value("General/uiFontSize").toInt() == 18, "general settings saved");
    const auto family = settings.value("General/uiFontFamily").toString();
    require(family == selectedFamily && window.styleSheet().contains("font-family: \"" + family + "\""), "UI font applied to window");
    require(settings.value("General/minimizeToTray").toBool() && !settings.value("General/checkUpdatesOnStartup").toBool(), "tray/update preferences saved");
    require(settings.value("editor/fontSize").toInt() == 19 && settings.value("editor/tabWidth").toInt() == 7, "editor settings saved");
    require(window.tray_->isVisible() == QSystemTrayIcon::isSystemTrayAvailable(), "tray coordinated by window");
    require(QCoreApplication::translate("MainWindow", "设置") == "设置", "language change waits for restart");
    const auto path = temp.filePath("note.md");
    QFile file(path);
    require(file.open(QIODevice::WriteOnly) && file.write("# Heading\n") > 0, "note fixture");
    file.close();
    window.openExternalFile(path);
    require(window.markdownEditor_->baseEditorFontPointSize() == 19 && window.markdownEditor_->getConfig().m_tabStopWidth == 7, "future editor uses saved settings");
    const auto secondPath = temp.filePath("second.md");
    require(QFile::copy(path, secondPath), "second note fixture");
    window.openExternalFile(secondPath);
    QTimer::singleShot(0, [&] {
        auto *dialog = qobject_cast<SettingsDialog *>(QApplication::activeModalWidget());
        dialog->findChild<QSpinBox *>("fontSize")->setValue(21);
        dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
    });
    window.showSettings();
    for (const auto &session: window.editorSessions_) require(session.editor->baseEditorFontPointSize() == 21, "all live editor settings applied");
    {
        SettingsDialog dialog(window.markdownEditor_, &window);
        bool requested = false;
        QObject::connect(&dialog, &SettingsDialog::updateCheckRequested, [&](QLabel *status) {
            require(status == dialog.findChild<QLabel *>("updateStatus"), "explicit update status interface");
            status->setText("Test update result");
            requested = true;
        });
        dialog.findChild<QPushButton *>("checkUpdates")->click();
        require(requested && dialog.findChild<QLabel *>("updateStatus")->text() == "Test update result", "update request wiring");
        dialog.reject();
    }
    QTranslator english;
    require(english.load(":/translations/qsnote_en_US.qm"), "English translations");
    app.installTranslator(&english);
    SettingsDialog translated(nullptr, &window);
    require(translated.findChild<QTabBar *>("settingsTabs")->tabText(0) == QCoreApplication::translate("MainWindow", "常用设置"), "settings translation context");
    require(translated.findChild<QTabWidget *>("editorSettingsTabs")->tabText(0) != "文本", "editor translation retained");
    qInfo() << "PASS: empty/live editor settings, save/cancel, UI font/size/language/tray/updates, restart semantics, update signal, English";
}
