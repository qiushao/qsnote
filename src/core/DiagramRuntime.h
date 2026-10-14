#ifndef QSNOTE_DIAGRAMRUNTIME_H
#define QSNOTE_DIAGRAMRUNTIME_H

#include <QProcessEnvironment>
#include <QStringList>

namespace DiagramRuntime {
struct Command {
    QString executable;
    QStringList arguments;
    QString error;
};

Command plantUml();
QString graphviz();
QProcessEnvironment environment();
}

#endif
