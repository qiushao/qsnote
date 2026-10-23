#include "ui/MainWindow.h"
#include "core/ApplicationLanguage.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>

int main(int argc, char *argv[]) {
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName("qsnote");
    QCoreApplication::setApplicationVersion(APP_VERSION);
    QCoreApplication::setOrganizationName("qiushao");
    QGuiApplication::setDesktopFileName("qsnote");
    QApplication::setWindowIcon(QIcon(":/icons/qsnote.png"));

    QCommandLineParser parser;
    parser.setApplicationDescription("QSNote");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.process(app);

    ApplicationLanguage language;

    MainWindow window;
    window.maximizeWindow();
    window.show();

    return QApplication::exec();
}
