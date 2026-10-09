#include "MainWindow.h"
#include <QCoreApplication>
#include <QWidget>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle(QCoreApplication::applicationName());
    resize(1000, 700);
    setCentralWidget(new QWidget(this));
}
