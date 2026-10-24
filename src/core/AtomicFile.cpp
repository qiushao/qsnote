#include "AtomicFile.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

bool AtomicFile::write(const QString &path, const QByteArray &data, QString *error) {
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        *error = QCoreApplication::translate("NotebookStore", "无法创建目录：") + QFileInfo(path).absolutePath();
        return false;
    }
    QFile current(path);
    if (current.open(QIODevice::ReadOnly)) {
        const auto bytes = current.readAll();
        if (current.error() == QFileDevice::NoError && bytes == data) return true;
    }
    current.close();
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        *error = path + ": " + file.errorString();
        return false;
    }
    return true;
}
