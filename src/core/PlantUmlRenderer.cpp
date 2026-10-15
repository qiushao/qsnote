#include "PlantUmlRenderer.h"
#include <QCoreApplication>
#include "DiagramRuntime.h"
#include <QImage>
#include <QRegularExpression>

PlantUmlRenderer::PlantUmlRenderer(QObject *parent) : QObject(parent) {
    timeout_.setSingleShot(true);
    connect(&timeout_, &QTimer::timeout, this, [this] { process_.kill(); });
    connect(&process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart) return;
        timeout_.stop();
        emit finished({}, QCoreApplication::translate("PlantUmlRenderer", "无法启动 PlantUML：") + process_.errorString());
    });
    connect(&process_, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus status) {
        const bool timedOut = !timeout_.isActive();
        timeout_.stop();
        const auto png = process_.readAllStandardOutput();
        if (timedOut) {
            emit finished({}, QCoreApplication::translate("PlantUmlRenderer", "PlantUML 渲染超时。"));
        } else if (status != QProcess::NormalExit || exitCode != 0 || QImage::fromData(png, "PNG").isNull()) {
            const auto details = QString::fromUtf8(process_.readAllStandardError()).trimmed().right(2000);
            emit finished({}, QCoreApplication::translate("PlantUmlRenderer", "PlantUML 渲染失败：") + (details.isEmpty() ? QCoreApplication::translate("PlantUmlRenderer", "未生成有效 PNG 图片。") : details));
        } else {
            emit finished(png, {});
        }
    });
}

void PlantUmlRenderer::render(const QString &source) {
    const auto command = DiagramRuntime::plantUml();
    auto executable = command.executable;
    if (executable.isEmpty()) {
        emit finished({}, command.error.isEmpty() ? QCoreApplication::translate("PlantUmlRenderer", "未找到 PlantUML，请重新安装完整应用包，或安装 PlantUML 和 Graphviz。") : command.error);
        return;
    }
    auto text = source;
    if (!QRegularExpression(R"(^\s*@start\w+\b)", QRegularExpression::MultilineOption).match(text).hasMatch()) {
        text = "@startuml\n" + text + "\n@enduml\n";
    }
    connect(&process_, &QProcess::started, this, [this, text] {
        process_.write(text.toUtf8());
        process_.closeWriteChannel();
    });
    timeout_.start(30000);
    auto arguments = command.arguments;
    arguments.append({"-pipe", "-tpng", "-charset", "UTF-8", "-failfast2"});
#ifdef Q_OS_WIN
    if (executable.endsWith(".bat", Qt::CaseInsensitive) || executable.endsWith(".cmd", Qt::CaseInsensitive)) {
        arguments.prepend(executable);
        arguments.prepend("/c");
        executable = qEnvironmentVariable("COMSPEC", "cmd.exe");
    }
#endif
    process_.setProcessEnvironment(DiagramRuntime::environment());
    process_.start(executable, arguments);
}
