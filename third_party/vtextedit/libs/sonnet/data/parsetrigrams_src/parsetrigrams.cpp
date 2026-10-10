/**
 * parsetrigrams.cpp
 *
 * Parse a set of trigram files into a QHash, and serialize to stdout.
 *
 * SPDX-FileCopyrightText: 2006 Jacob Rideout <kde@jacobrideout.net>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QRegularExpression>
#include <QString>
#include <QTextStream>
#include <QDebug>

int main(int argc, char **argv)
{
    if (argc < 3) {
        qWarning() << argv[0] << "trigrams_files outfile.trigrams.map";
        return -1;
    }

    QFile sout(QString::fromLocal8Bit(argv[2]));
    if (!sout.open(QIODevice::WriteOnly)) {
        qWarning() << "Unable to open output file" << argv[2];
        return -1;
    }

    QDataStream out(&sout);
    out.setVersion(QDataStream::Qt_5_12);

    QString path = QLatin1String(argv[1]);
    QDir td(path);

    QHash<QString, QHash<QString, int>> models;

    const QRegularExpression rx(QStringLiteral("(?:.{3})\\s+(.*)"));
    for (const QString &fname : td.entryList(QDir::Files)) {
        QFile fin(td.filePath(fname));
        fin.open(QFile::ReadOnly | QFile::Text);
        QTextStream stream(&fin);

#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
        // For Qt 6, it is UTF-8 by default.
        stream.setCodec("UTF-8");
#endif
        while (!stream.atEnd()) {
            QString line = stream.readLine();
            const QRegularExpressionMatch match = rx.match(line);
            if (match.hasMatch()) {
                models[fname][line.left(3)] = match.captured(1).toInt();
            }
        }
    }

    out << models;
}
