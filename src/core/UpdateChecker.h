#ifndef QSNOTE_UPDATECHECKER_H
#define QSNOTE_UPDATECHECKER_H

#include <QObject>

class UpdateChecker : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    void check();
    struct Result {
        QString version;
        QString error;
    };
    static Result parseRelease(const QByteArray &data, const QString &currentVersion);

signals:
    void finished(const QString &version, const QString &error);

private:
    bool checking_ = false;
};

#endif
