#pragma once

#include <QCache>
#include <QObject>
#include <QProcess>
#include <QTemporaryDir>
#include <QTimer>

// All execution contexts are supplied by native notebook owners, never by WebChannel.
class PyplotEnvironment : public QObject {
    Q_OBJECT
public:
    static PyplotEnvironment &instance();
    static QString canonicalRoot(const QString &root);
    static QString interpreter(const QString &configured);
    static QString interpreter();
    static bool enabled();
    static QString executionError(const QString &root, const QString &directory);
    void setEnabled(bool enabled);
    void setInterpreter(const QString &path);
    void refresh();
    QCache<QString, QByteArray> cache{16384};
signals:
    void changed();
};

class PyplotRenderer : public QObject {
    Q_OBJECT
public:
    explicit PyplotRenderer(QObject *parent = nullptr);
    ~PyplotRenderer() override;
    void render(const QString &source, const QString &notebookRoot, const QString &directory, bool useCache = true);
    void cancel();
signals:
    void finished(const QByteArray &png, const QString &error);
private:
    void complete(const QByteArray &png, const QString &error);
    QProcess process_;
    QTimer timeout_;
    QTemporaryDir temporary_;
    QString key_;
    QString diagnostics_;
    bool running_ = false;
};
