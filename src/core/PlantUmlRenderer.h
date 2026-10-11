#ifndef QSNOTE_PLANTUMLRENDERER_H
#define QSNOTE_PLANTUMLRENDERER_H

#include <QObject>
#include <QProcess>
#include <QTimer>

class PlantUmlRenderer : public QObject {
    Q_OBJECT
public:
    explicit PlantUmlRenderer(QObject *parent = nullptr);
    void render(const QString &source);
signals:
    void finished(const QByteArray &png, const QString &error);
private:
    QProcess process_;
    QTimer timeout_;
};

#endif
