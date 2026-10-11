#include "PlantUmlRenderer.h"
#include <QImage>
#include <QRegularExpression>
#include <QStandardPaths>

PlantUmlRenderer::PlantUmlRenderer(QObject *parent) : QObject(parent) {
    timeout_.setSingleShot(true);
    connect(&timeout_, &QTimer::timeout, this, [this] { process_.kill(); });
    connect(&process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart) return;
        timeout_.stop();
        emit finished({}, "无法启动 PlantUML：" + process_.errorString());
    });
    connect(&process_, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus status) {
        const bool timedOut = !timeout_.isActive();
        timeout_.stop();
        const auto png = process_.readAllStandardOutput();
        if (timedOut) {
            emit finished({}, "PlantUML 渲染超时。");
        } else if (status != QProcess::NormalExit || exitCode != 0 || QImage::fromData(png, "PNG").isNull()) {
            const auto details = QString::fromUtf8(process_.readAllStandardError()).trimmed().right(2000);
            emit finished({}, "PlantUML 渲染失败：" + (details.isEmpty() ? QStringLiteral("未生成有效 PNG 图片。") : details));
        } else {
            emit finished(png, {});
        }
    });
}

void PlantUmlRenderer::render(const QString &source) {
    auto executable = QStandardPaths::findExecutable("plantuml");
    if (executable.isEmpty()) {
        emit finished({}, "未找到 PlantUML，请安装 PlantUML 和 Graphviz，并将 plantuml 加入 PATH。");
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
    QStringList arguments{"-pipe", "-tpng", "-charset", "UTF-8", "-failfast2"};
#ifdef Q_OS_WIN
    if (executable.endsWith(".bat", Qt::CaseInsensitive) || executable.endsWith(".cmd", Qt::CaseInsensitive)) {
        arguments.prepend(executable);
        arguments.prepend("/c");
        executable = qEnvironmentVariable("COMSPEC", "cmd.exe");
    }
#endif
    process_.start(executable, arguments);
}
