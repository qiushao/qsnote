#include "ui/MainWindow.h"
#include <QApplication>
#include <QCommandLineParser>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName("qsnote");
    QCoreApplication::setApplicationVersion(APP_VERSION);
    QCoreApplication::setOrganizationName("qiushao");

    QCommandLineParser parser;
    parser.setApplicationDescription("QSNote");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.process(app);

    MainWindow window;
    window.show();

    return QApplication::exec();
}
