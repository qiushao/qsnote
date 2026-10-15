#include "DiagramRuntime.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace {
QString findExecutable(const QString &name) {
    auto executable = QStandardPaths::findExecutable(name);
#ifdef Q_OS_MACOS
    // Finder launches do not inherit the user's shell PATH.
    if (executable.isEmpty()) executable = QStandardPaths::findExecutable(name, {"/opt/homebrew/bin", "/usr/local/bin"});
#endif
    return executable;
}

#ifdef Q_OS_WIN
QString bundledFile(const QString &path) {
    const auto file = QCoreApplication::applicationDirPath() + "/runtime/" + path;
    return QFileInfo(file).isFile() ? file : QString();
}
#endif
}

DiagramRuntime::Command DiagramRuntime::plantUml() {
#ifdef Q_OS_WIN
    const auto jar = bundledFile("plantuml/plantuml.jar");
    if (!jar.isEmpty()) {
        auto java = QStandardPaths::findExecutable("java");
        const auto javaHome = qEnvironmentVariable("JAVA_HOME");
        if (java.isEmpty() && !javaHome.isEmpty()) java = QStandardPaths::findExecutable("java", {javaHome + "/bin"});
        if (java.isEmpty()) return {{}, {}, QCoreApplication::translate("DiagramRuntime", "未找到 Java，请安装 Java，并将 java 加入 PATH 或设置 JAVA_HOME。")};
        return {java, {"-Djava.awt.headless=true", "-Dfile.encoding=UTF-8", "-jar", QDir::toNativeSeparators(jar)}, {}};
    }
#endif
    return {findExecutable("plantuml"), {}, {}};
}

QString DiagramRuntime::graphviz() {
#ifdef Q_OS_WIN
    const auto dot = bundledFile("graphviz/bin/dot.exe");
    if (!dot.isEmpty()) return dot;
#endif
    return findExecutable("dot");
}

QProcessEnvironment DiagramRuntime::environment() {
    auto result = QProcessEnvironment::systemEnvironment();
    const auto dot = graphviz();
    if (!dot.isEmpty()) {
        result.insert("GRAPHVIZ_DOT", QDir::toNativeSeparators(dot));
        result.insert("PATH", QDir::toNativeSeparators(QFileInfo(dot).absolutePath()) + QDir::listSeparator() + result.value("PATH"));
    }
    return result;
}
