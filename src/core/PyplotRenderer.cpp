#include "PyplotRenderer.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QStandardPaths>

PyplotEnvironment &PyplotEnvironment::instance() {
    static PyplotEnvironment environment;
    return environment;
}

QString PyplotEnvironment::canonicalRoot(const QString &root) {
    return root.isEmpty() ? QString() : QFileInfo(root).canonicalFilePath();
}

QString PyplotEnvironment::interpreter(const QString &configured) {
    if (!configured.isEmpty()) return configured;
    auto executable = QStandardPaths::findExecutable("python3");
    if (executable.isEmpty()) executable = QStandardPaths::findExecutable("python");
    return executable;
}

QString PyplotEnvironment::interpreter() {
    return interpreter(QSettings().value("pyplot/interpreter").toString());
}

bool PyplotEnvironment::enabled() {
    return QSettings().value("pyplot/enabled", false).toBool();
}

QString PyplotEnvironment::executionError(const QString &root, const QString &directory) {
    const auto canonical = canonicalRoot(root);
    const auto noteDirectory = canonicalRoot(directory);
    if (canonical.isEmpty() || noteDirectory.isEmpty() ||
        (noteDirectory != canonical && !noteDirectory.startsWith(canonical + '/'))) {
        return tr("Python 绘图仅支持已登记笔记本内的笔记。");
    }
    if (!enabled()) return tr("请先在“设置 → Python 绘图”中启用 Python 绘图。");
    return {};
}

void PyplotEnvironment::setEnabled(bool enabled) {
    if (PyplotEnvironment::enabled() == enabled) return;
    QSettings().setValue("pyplot/enabled", enabled);
    refresh();
}

void PyplotEnvironment::setInterpreter(const QString &path) {
    if (QSettings().value("pyplot/interpreter").toString() == path) return;
    QSettings().setValue("pyplot/interpreter", path);
    refresh();
}

void PyplotEnvironment::refresh() {
    cache.clear();
    emit changed();
}

PyplotRenderer::PyplotRenderer(QObject *parent) : QObject(parent) {
    timeout_.setSingleShot(true);
    connect(&timeout_, &QTimer::timeout, this, [this] {
        process_.kill();
        complete({}, tr("Python 绘图超时（30 秒）。"));
    });
    connect(&PyplotEnvironment::instance(), &PyplotEnvironment::changed, this, &PyplotRenderer::cancel);
    connect(&process_, &QProcess::readyReadStandardOutput, this, [this] { process_.readAllStandardOutput(); });
    connect(&process_, &QProcess::readyReadStandardError, this, [this] {
        diagnostics_ = (diagnostics_ + QString::fromUtf8(process_.readAllStandardError())).right(4000);
    });
    connect(&process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) complete({}, tr("无法启动 Python：") + process_.errorString());
    });
    connect(&process_, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus status) {
        if (!running_) return;
        QFile result(temporary_.filePath("result.json"));
        const auto data = result.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(result.readAll()).object() : QJsonObject();
        const auto png = QByteArray::fromBase64(data.value("png").toString().toLatin1());
        if (status == QProcess::NormalExit && exitCode == 0 && !QImage::fromData(png, "PNG").isNull()) {
            if (!key_.isEmpty()) PyplotEnvironment::instance().cache.insert(key_, new QByteArray(png), qMax(1, static_cast<int>(png.size() / 1024)));
            complete(png, {});
        } else {
            complete({}, data.value("error").toString(tr("Python 绘图失败：未生成有效 PNG。")) + '\n' + diagnostics_);
        }
    });
}

PyplotRenderer::~PyplotRenderer() {
    process_.disconnect(this);
    if (process_.state() != QProcess::NotRunning) {
        process_.kill();
        process_.waitForFinished();
    }
}

void PyplotRenderer::complete(const QByteArray &png, const QString &error) {
    if (!running_) return;
    running_ = false;
    timeout_.stop();
    emit finished(png, error);
}

void PyplotRenderer::cancel() {
    process_.kill();
    complete({}, tr("Python 绘图已取消。"));
}

void PyplotRenderer::render(const QString &source, const QString &notebookRoot, const QString &directory, bool useCache) {
    running_ = true;
    const auto error = PyplotEnvironment::executionError(notebookRoot, directory);
    if (!error.isEmpty()) { complete({}, error); return; }
    const auto executable = PyplotEnvironment::interpreter();
    if (executable.isEmpty()) { complete({}, tr("未找到 Python 3，请在设置中选择解释器。")); return; }
    if (useCache) {
        const QJsonObject identity{{"source", source}, {"interpreter", executable}, {"directory", PyplotEnvironment::canonicalRoot(directory)}};
        key_ = QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(identity).toJson(), QCryptographicHash::Sha256).toHex());
        if (const auto *png = PyplotEnvironment::instance().cache.object(key_)) { complete(*png, {}); return; }
    }
    QFile script(":/python/pyplot.py");
    if (!temporary_.isValid() || !script.open(QIODevice::ReadOnly)) { complete({}, tr("无法创建 Python 绘图运行环境。")); return; }
    process_.setWorkingDirectory(directory);
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert("PYTHONIOENCODING", "utf-8");
    environment.insert("MPLBACKEND", "Agg");
    process_.setProcessEnvironment(environment);
    connect(&process_, &QProcess::started, this, [this, source] {
        process_.write(source.toUtf8());
        process_.closeWriteChannel();
    });
    timeout_.start(30000);
    process_.start(executable, {"-c", QString::fromUtf8(script.readAll()), temporary_.filePath("result.json")});
}
