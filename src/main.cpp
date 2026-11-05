#include "core/ApplicationLanguage.h"
#include "core/DesktopSession.h"
#include "ui/MainWindow.h"
#include <QCommandLineParser>
#include <QFileOpenEvent>
#include <QIcon>
#include <QPointer>
#include <singleapplication.h>

#ifdef Q_OS_WIN
#include <QDir>
#include <QSettings>
#include <qt_windows.h>
#include <shlobj.h>
#endif

namespace {
class Application : public SingleApplication {
public:
    using SingleApplication::SingleApplication;

    void setMainWindow(MainWindow *window) {
        window_ = window;
        for (const auto &path: pendingFiles_) window_->openFile(path);
        pendingFiles_.clear();
    }

    void openFiles(const QStringList &paths) {
        if (!window_) {
            pendingFiles_.append(paths);
            return;
        }
        for (const auto &path: paths) window_->openFile(path);
        window_->raise();
        window_->activateWindow();
    }

protected:
    bool event(QEvent *event) override {
        if (event->type() == QEvent::FileOpen) {
            const auto *openEvent = dynamic_cast<QFileOpenEvent *>(event);
            if (!openEvent->url().isLocalFile()) return false;
            openFiles({openEvent->url().toLocalFile()});
            return true;
        }
        return SingleApplication::event(event);
    }

private:
    QPointer<MainWindow> window_;
    QStringList pendingFiles_;
};

#ifdef Q_OS_WIN
void registerMarkdownFiles() {
    const auto executable = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    const auto command = QStringLiteral("\"%1\" -- \"%2\"").arg(executable, QStringLiteral("%1"));
    QSettings classes(QStringLiteral("HKEY_CURRENT_USER\\Software\\Classes"), QSettings::NativeFormat);
    const QHash<QString, QString> entries = {
            {"QSNote.Markdown/.", "Markdown Document"},
            {"QSNote.Markdown/DefaultIcon/.", QStringLiteral("\"%1\",0").arg(executable)},
            {"QSNote.Markdown/shell/open/command/.", command},
            {".md/OpenWithProgids/QSNote.Markdown", QString()},
            {"Applications/qsnote.exe/FriendlyAppName", "QSNote"},
            {"Applications/qsnote.exe/SupportedTypes/.md", QString()},
            {"Applications/qsnote.exe/shell/open/command/.", command}};
    bool changed = false;
    for (auto it = entries.cbegin(); it != entries.cend(); ++it) {
        if (classes.contains(it.key()) && classes.value(it.key()).toString() == it.value()) continue;
        classes.setValue(it.key(), it.value());
        changed = true;
    }
    classes.sync();
    if (changed && classes.status() == QSettings::NoError) SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
}
#endif
}// namespace

int main(int argc, char *argv[]) {
    // Normalize before Qt initializes D-Bus or starts external applications.
    DesktopSession::normalizeBusAddress();
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QCoreApplication::setApplicationName("qsnote");
    QCoreApplication::setOrganizationName("qiushao");
    Application app(argc, argv, true, Application::Mode::User | Application::Mode::SecondaryNotification);
    QCoreApplication::setApplicationVersion(APP_VERSION);
    QGuiApplication::setDesktopFileName("qsnote");
    QApplication::setWindowIcon(QIcon(":/icons/qsnote.png"));

    QCommandLineParser parser;
    parser.setApplicationDescription("QSNote");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("files", "Markdown files to open.", "[files...]");
    parser.process(app);

#ifdef Q_OS_WIN
    registerMarkdownFiles();
#endif

    if (app.isSecondary()) {
        app.sendMessage(parser.positionalArguments().join(QLatin1Char('\n')).toUtf8());
        return 0;
    }

    QObject::connect(&app, &SingleApplication::receivedMessage,
                     [&app](quint32, const QByteArray &message) {
                         app.openFiles(QString::fromUtf8(message).split(QLatin1Char('\n'), Qt::SkipEmptyParts));
                     });

    ApplicationLanguage language;

    MainWindow window;
    window.maximizeWindow();
    window.show();
    app.setMainWindow(&window);
    for (const auto &path: parser.positionalArguments()) window.openFile(path);

    return QApplication::exec();
}
