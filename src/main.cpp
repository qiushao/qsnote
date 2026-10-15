#include "ui/MainWindow.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QLocale>
#include <QSettings>
#include <QTranslator>

int main(int argc, char *argv[]) {
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName("qsnote");
    QCoreApplication::setApplicationVersion(APP_VERSION);
    QCoreApplication::setOrganizationName("qiushao");
    QApplication::setWindowIcon(QIcon(":/icons/qsnote.png"));

    QCommandLineParser parser;
    parser.setApplicationDescription("QSNote");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.process(app);

    const auto language = QSettings().value("general/language", "system").toString();
    QLocale::setDefault(language == "zh_CN" ? QLocale("zh_CN") : QLocale::system());
    // Chinese is the fallback until other application translations are available.
    QTranslator editorTranslator;
    if (editorTranslator.load(":/translations/vtextedit_zh_CN.qm")) QCoreApplication::installTranslator(&editorTranslator);

    MainWindow window;
    window.showMaximized();

    return QApplication::exec();
}
