#include "core/DiagramRuntime.h"
#include "core/PlantUmlRenderer.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QProcess>
#include <QSignalSpy>
#include <QScopeGuard>
#include <QTest>

// NOLINTBEGIN(readability-convert-member-functions-to-static)
class DiagramRuntimeTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
#ifdef Q_OS_WIN
        const auto java = DiagramRuntime::plantUml().executable;
        QVERIFY2(!java.isEmpty(), "Install system Java and set PATH or JAVA_HOME");
        // Keep only system Java through JAVA_HOME; no diagram tools on PATH.
        qputenv("JAVA_HOME", QFileInfo(java).dir().absoluteFilePath("..").toUtf8());
        qputenv("PATH", QByteArray());
        qunsetenv("GRAPHVIZ_DOT");
        const auto root = QCoreApplication::applicationDirPath() + "/runtime/";
        QVERIFY(!QFileInfo::exists(root + "java"));
        QVERIFY(!DiagramRuntime::plantUml().executable.startsWith(root));
        QVERIFY(DiagramRuntime::plantUml().arguments.contains(QDir::toNativeSeparators(root + "plantuml/plantuml.jar")));
        QVERIFY(DiagramRuntime::graphviz().startsWith(root));
#elif defined(Q_OS_MACOS)
        // Reproduce Finder's environment, without Homebrew's shell setup.
        qputenv("PATH", "/usr/bin:/bin:/usr/sbin:/sbin");
#endif
        QVERIFY2(!DiagramRuntime::plantUml().executable.isEmpty(), "PlantUML runtime is required for diagram integration tests");
        QVERIFY2(!DiagramRuntime::graphviz().isEmpty(), "Graphviz runtime is required for diagram integration tests");
    }

#ifdef Q_OS_WIN
    void javaOnPath() {
        const auto home = qgetenv("JAVA_HOME");
        const auto path = qgetenv("PATH");
        const auto restore = qScopeGuard([home, path] { qputenv("JAVA_HOME", home); qputenv("PATH", path); });
        const auto expected = QFileInfo(DiagramRuntime::plantUml().executable).canonicalFilePath();
        qputenv("PATH", QFileInfo(expected).absolutePath().toUtf8());
        qunsetenv("JAVA_HOME");
        QCOMPARE(QFileInfo(DiagramRuntime::plantUml().executable).canonicalFilePath(), expected);
    }

    void missingJava() {
        const auto home = qgetenv("JAVA_HOME");
        const auto restore = qScopeGuard([home] { qputenv("JAVA_HOME", home); });
        qunsetenv("JAVA_HOME");
        QVERIFY(DiagramRuntime::plantUml().executable.isEmpty());
        PlantUmlRenderer renderer;
        QSignalSpy result(&renderer, &PlantUmlRenderer::finished);
        renderer.render("Alice -> Bob");
        QCOMPARE(result.size(), 1);
        QVERIFY(result.first()[0].toByteArray().isEmpty());
        QVERIFY(result.first()[1].toString().contains("未找到 Java"));
    }
#endif

    void plantUml_data() { // NOLINT(readability-identifier-naming): Qt Test data-provider convention.
        QTest::addColumn<QString>("source");
        QTest::newRow("sequence") << QString("Alice -> Bob: hello");
        QTest::newRow("class-graphviz") << QString("@startuml\nclass Parent\nclass Child\nParent <|-- Child\n@enduml");
    }

    void plantUml() {
        QFETCH(QString, source);
        PlantUmlRenderer renderer;
        QSignalSpy result(&renderer, &PlantUmlRenderer::finished);
        renderer.render(source);
        QTRY_COMPARE_WITH_TIMEOUT(result.size(), 1, 45000);
        QVERIFY2(result.first()[1].toString().isEmpty(), qPrintable(result.first()[1].toString()));
        const auto image = QImage::fromData(result.first()[0].toByteArray(), "PNG");
        QVERIFY(image.width() > 20 && image.height() > 20);
    }

    void graphvizSfdp() {
        QProcess process;
        process.setProcessEnvironment(DiagramRuntime::environment());
        process.start(DiagramRuntime::graphviz(), {"-Ksfdp", "-Tpng"});
        QVERIFY(process.waitForStarted());
        process.write("digraph G { a -> b; b -> c; c -> a; }");
        process.closeWriteChannel();
        QVERIFY(process.waitForFinished(30000));
        QCOMPARE(process.exitStatus(), QProcess::NormalExit);
        QVERIFY2(process.exitCode() == 0, process.readAllStandardError().constData());
        const auto image = QImage::fromData(process.readAllStandardOutput(), "PNG");
        QVERIFY(image.width() > 20 && image.height() > 20);
    }
};
// NOLINTEND(readability-convert-member-functions-to-static)

QTEST_GUILESS_MAIN(DiagramRuntimeTest)
#include "DiagramRuntimeTest.moc"
