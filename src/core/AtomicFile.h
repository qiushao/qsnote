#pragma once

#include <QByteArray>
#include <QString>

namespace AtomicFile {
// Creates parent directories and preserves timestamps when the bytes are unchanged.
bool write(const QString &path, const QByteArray &data, QString *error);
}// namespace AtomicFile
