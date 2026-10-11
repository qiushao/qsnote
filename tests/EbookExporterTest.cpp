#include "core/EbookExporter.h"
#include <QDir>
#include <QFile>
#include <QImage>
#include <QProcess>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>
#include <QXmlStreamReader>
#include <QtGui/private/qzipreader_p.h>

namespace {
QByteArray readFile(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    return file.readAll();
}
Notebook fixture(const QString &root) {
    QDir().mkpath(root + "/chapters");
    QImage image(80, 40, QImage::Format_RGB32);
    image.fill(Qt::red);
    image.save(root + "/图 片.png");
    return {"book", "中文电子书 & 示例", "ebook", {
        {"one", {}, "第一章", "# 开始\n\n第一章内容。![图片](../图%20片.png)\n\n[下一章](second.md#开始)\n\n|列一|列二|\n|---|---|\n|甲|乙|\n\n- [x] 完成\n\n脚注[^a]\n\n[^a]: 第一条脚注", false, {}, "chapters/first.md"},
        {"last", {}, "末章", "最后章节。", false, {}, "last.md"},
        {"two", "one", "第二章", "# 开始\n\n第二章内容。\n\n[返回](first.md#开始)\n\n脚注[^a]\n\n[^a]: 第二条脚注", false, {}, "chapters/second.md"},
        {"group", {}, "分组", {}, true, {}, "group"},
        {"three", "group", "第三章", "第三章内容。", false, {}, "group/third.md"}
    }, {}, root};
}
}// namespace

// NOLINTBEGIN(readability-convert-member-functions-to-static)
class EbookExporterTest : public QObject {
    Q_OBJECT
private slots:
    void htmlScopeOrderAssetsAndCollisions() {
        QTemporaryDir source;
        QTemporaryDir destination;
        const auto book = fixture(source.path());
        QString error;
        const auto output = EbookExporter::exportBook(book, {}, EbookExporter::Format::Html, destination.path(), &error);
        QVERIFY2(!output.isEmpty(), qPrintable(error));
        const auto html = QString::fromUtf8(readFile(output));
        QVERIFY(html.contains("<body><nav epub:type=\"toc\">"));
        QVERIFY(html.contains("第一章内容"));
        QVERIFY(html.contains("data:image/png;base64,"));
        QVERIFY(html.contains("href=\"#chapter-2-%E5%BC%80%E5%A7%8B\""));
        QVERIFY(html.contains("id=\"chapter-2-开始\""));
        QVERIFY(html.indexOf("第一章内容") < html.indexOf("第二章内容"));
        QVERIFY(html.indexOf("第二章内容") < html.indexOf("最后章节"));
        QVERIFY(html.contains("<table>"));
        QXmlStreamReader xml(html);
        while (!xml.atEnd()) xml.readNext();
        QVERIFY2(!xml.hasError(), qPrintable(xml.errorString()));
        const auto single = EbookExporter::exportBook(book, "one", EbookExporter::Format::Html, destination.path(), &error);
        QVERIFY2(!single.isEmpty(), qPrintable(error));
        QVERIFY(readFile(single).contains("<body><nav epub:type=\"toc\">"));
        QVERIFY(readFile(single).contains(QString("第一章内容").toUtf8()));
        QVERIFY(!readFile(single).contains(QString("第二章内容").toUtf8()));
        const auto second = EbookExporter::exportBook(book, "one", EbookExporter::Format::Html, destination.path(), &error);
        QVERIFY2(!second.isEmpty(), qPrintable(error));
        QVERIFY(second.endsWith("-2.html"));
        QCOMPARE(readFile(single), readFile(second));
    }

    void epubPackageAndNavigation() {
        QTemporaryDir source;
        QTemporaryDir destination;
        const auto book = fixture(source.path());
        QString error;
        for (const auto &id: {QString(), QString("one")}) {
            const auto output = EbookExporter::exportBook(book, id, EbookExporter::Format::Epub, destination.path(), &error);
            QVERIFY2(!output.isEmpty(), qPrintable(error));
            QZipReader zip(output);
            QCOMPARE(zip.status(), QZipReader::NoError);
            QCOMPARE(zip.fileData("mimetype"), QByteArray("application/epub+zip"));
            const auto raw = readFile(output);
            QCOMPARE(raw.mid(8, 2), QByteArray(2, '\0')); // mimetype is stored, not compressed
            QCOMPARE(raw.mid(30, 8), QByteArray("mimetype"));
            for (const auto &name: {"META-INF/container.xml", "content.opf", "book.xhtml", "nav.xhtml", "toc.ncx"}) {
                const auto data = zip.fileData(name);
                QVERIFY(!data.isEmpty());
                QXmlStreamReader xml(data);
                while (!xml.atEnd()) xml.readNext();
                QVERIFY2(!xml.hasError(), qPrintable(QString(name) + ": " + xml.errorString()));
            }
            QVERIFY(!QImage::fromData(zip.fileData("image-1.png")).isNull());
            const auto content = zip.fileData("book.xhtml");
            QVERIFY(content.contains("<body><nav epub:type=\"toc\">"));
            QVERIFY(content.contains(QString("第一章内容").toUtf8()));
            QCOMPARE(content.contains(QString("第二章内容").toUtf8()), id.isEmpty());
            const auto nav = zip.fileData("nav.xhtml");
            QVERIFY(nav.contains("book.xhtml#chapter-1"));
            QVERIFY(zip.fileData("toc.ncx").contains("<navMap><navPoint id=\"chapter-1\""));
            if (id.isEmpty()) {
                QVERIFY(nav.contains(QString("第一章</a><ol><li><a href=\"book.xhtml#chapter-2\">第二章").toUtf8()));
                QVERIFY(nav.contains(QString("分组</a><ol><li><a href=\"book.xhtml#chapter-5\">第三章").toUtf8()));
            }
            if (qEnvironmentVariableIsSet("QSNOTE_EXPORT_ARTIFACTS")) {
                QDir().mkpath(qEnvironmentVariable("QSNOTE_EXPORT_ARTIFACTS"));
                QVERIFY(QFile::copy(output, qEnvironmentVariable("QSNOTE_EXPORT_ARTIFACTS") + (id.isEmpty() ? "/book.epub" : "/single.epub")));
            }
        }
    }

    void pdfContainsChineseAndImages() {
        QTemporaryDir source;
        QTemporaryDir destination;
        const auto book = fixture(source.path());
        QString error;
        for (const auto &id: {QString(), QString("one")}) {
            const auto output = EbookExporter::exportBook(book, id, EbookExporter::Format::Pdf, destination.path(), &error);
            QVERIFY2(!output.isEmpty(), qPrintable(error));
            QVERIFY(readFile(output).startsWith("%PDF-"));
            if (!QStandardPaths::findExecutable("pdftotext").isEmpty()) {
                QProcess process;
                process.start("pdftotext", {output, "-"});
                QVERIFY(process.waitForFinished());
                QCOMPARE(process.exitCode(), 0);
                const auto text = QString::fromUtf8(process.readAllStandardOutput());
                QVERIFY(text.trimmed().startsWith("目录"));
                QVERIFY(text.contains("第一章内容"));
                QCOMPARE(text.contains("第二章内容"), id.isEmpty());
                QVERIFY(!text.contains(QChar::ReplacementCharacter));
            }
            if (qEnvironmentVariableIsSet("QSNOTE_EXPORT_ARTIFACTS")) {
                QVERIFY(QFile::copy(output, qEnvironmentVariable("QSNOTE_EXPORT_ARTIFACTS") + (id.isEmpty() ? "/book.pdf" : "/single.pdf")));
            }
        }
    }

    void pdfBookmarksFollowNoteTree() {
        QTemporaryDir source;
        QTemporaryDir destination;
        auto book = fixture(source.path());
        book.notes.append({"deep", "two", "三级 (书签) \\ 示例 📚", "三级正文。", false, {}, "deep.md"});
        book.notes.append({"sibling", "one", "同级章节", "同级正文。", false, {}, "sibling.md"});
        for (int i = 0; i < 150; ++i) book.notes[0].content += "\n\n长章节的正文，用于验证跨页后的书签定位。";
        QString error;
        for (const auto &id: {QString(), QString("deep")}) {
            const auto output = EbookExporter::exportBook(book, id, EbookExporter::Format::Pdf, destination.path(), &error);
            QVERIFY2(!output.isEmpty(), qPrintable(error));
            const auto pdf = QString::fromLatin1(readFile(output));
            QVERIFY(pdf.contains("/PageMode /UseOutlines"));
            const auto outlines = QRegularExpression(R"((\d+) 0 obj\s*<< /Type /Outlines\s+/Count (\d+)\s+/First (\d+) 0 R\s+/Last (\d+) 0 R\s*>>)").match(pdf);
            QVERIFY(outlines.hasMatch());
            const int outlineId = outlines.captured(1).toInt();
            const QList<int> indices = id.isEmpty() ? QList<int>{0, 2, 5, 6, 1, 3, 4} : QList<int>{5};
            const QList<int> parents = id.isEmpty() ? QList<int>{-1, 0, 1, 0, -1, -1, 5} : QList<int>{-1};
            QCOMPARE(outlines.captured(2).toInt(), indices.size());
            auto items = QRegularExpression(R"((\d+) 0 obj\s*<< /Title <([0-9a-f]+)>(.*?)>>)", QRegularExpression::DotMatchesEverythingOption).globalMatch(pdf);
            int count = 0;
            while (items.hasNext()) {
                const auto item = items.next();
                QVERIFY(count < indices.size());
                QByteArray title = QByteArray::fromHex("feff");
                for (const auto character: book.notes[indices[count]].title) {
                    title.append(char(character.unicode() >> 8));
                    title.append(char(character.unicode() & 0xff));
                }
                QCOMPARE(item.captured(2).toLatin1(), title.toHex());
                QCOMPARE(item.captured(1).toInt(), outlineId + count + 1);
                const auto fields = item.captured(3);
                QVERIFY(fields.contains(QString("/Parent %1 0 R").arg(outlineId + parents[count] + 1)));
                QVERIFY(fields.contains(QString("/Dest /chapter-%1").arg(count + 1)));
                const auto target = QRegularExpression(QString(R"(/chapter-%1\s+\[(\d+) 0 R /XYZ\b)").arg(count + 1)).match(pdf);
                QVERIFY(target.hasMatch());
                QVERIFY(QRegularExpression(QString(R"(\b%1 0 obj\s*<<\s*/Type /Page\b)").arg(target.captured(1))).match(pdf).hasMatch());
                if (id.isEmpty() && count == 0) {
                    QVERIFY(fields.contains("/Count 3"));
                    QVERIFY(fields.contains(QString("/First %1 0 R").arg(outlineId + 2)));
                    QVERIFY(fields.contains(QString("/Last %1 0 R").arg(outlineId + 4)));
                    QVERIFY(fields.contains(QString("/Next %1 0 R").arg(outlineId + 5)));
                }
                if (id.isEmpty() && count == 3) QVERIFY(fields.contains(QString("/Prev %1 0 R").arg(outlineId + 2)));
                ++count;
            }
            QCOMPARE(count, indices.size());
            if (qEnvironmentVariableIsSet("QSNOTE_EXPORT_ARTIFACTS")) {
                QDir().mkpath(qEnvironmentVariable("QSNOTE_EXPORT_ARTIFACTS"));
                QVERIFY(QFile::copy(output, qEnvironmentVariable("QSNOTE_EXPORT_ARTIFACTS") + (id.isEmpty() ? "/bookmarks.pdf" : "/single-bookmark.pdf")));
            }
        }
    }

    void chmCompilation() {
        if (EbookExporter::chmCompiler().isEmpty()) QSKIP("Install chmcmd or hhc to run CHM integration test");
        QTemporaryDir source;
        QTemporaryDir destination;
        QString error;
        for (const auto &id: {QString(), QString("one")}) {
            const auto output = EbookExporter::exportBook(fixture(source.path()), id, EbookExporter::Format::Chm, destination.path(), &error);
            QVERIFY2(!output.isEmpty(), qPrintable(error));
            QVERIFY(readFile(output).startsWith("ITSF"));
            if (!QStandardPaths::findExecutable("7z").isEmpty()) {
                QProcess process;
                process.start("7z", {"x", "-so", output, "book.html"});
                QVERIFY(process.waitForFinished());
                QCOMPARE(process.exitCode(), 0);
                const auto html = QString::fromUtf8(process.readAllStandardOutput());
                QVERIFY(html.contains("<body><nav epub:type=\"toc\">"));
                QVERIFY(html.contains("第一章内容"));
                QCOMPARE(html.contains("第二章内容"), id.isEmpty());
            }
            if (qEnvironmentVariableIsSet("QSNOTE_EXPORT_ARTIFACTS")) {
                QVERIFY(QFile::copy(output, qEnvironmentVariable("QSNOTE_EXPORT_ARTIFACTS") + (id.isEmpty() ? "/book.chm" : "/single.chm")));
            }
        }
    }

    void embeddedAndDownloadedImages() {
        QTemporaryDir source;
        QTemporaryDir destination;
        auto book = fixture(source.path());
        const auto png = readFile(source.path() + "/图 片.png");
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        int requests = 0;
        connect(&server, &QTcpServer::newConnection, &server, [&] {
            auto *socket = server.nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, socket, [&, socket] {
                socket->readAll();
                ++requests;
                socket->write("HTTP/1.1 200 OK\r\nContent-Type: image/png\r\nContent-Length: " + QByteArray::number(png.size()) + "\r\nConnection: close\r\n\r\n" + png);
                socket->disconnectFromHost();
            });
        });
        const auto url = "http://127.0.0.1:" + QString::number(server.serverPort()) + "/image.png";
        book.notes[0].content = "![内嵌](data:image/png;base64," + QString::fromLatin1(png.toBase64()) + ")\n\n![远程](" + url + ")\n\n![重复](" + url + ")";
        QString error;
        const auto output = EbookExporter::exportBook(book, "one", EbookExporter::Format::Epub, destination.path(), &error);
        QVERIFY2(!output.isEmpty(), qPrintable(error));
        QCOMPARE(requests, 1);
        QZipReader zip(output);
        QVERIFY(!QImage::fromData(zip.fileData("image-1.png")).isNull());
        QVERIFY(!QImage::fromData(zip.fileData("image-2.png")).isNull());
        QVERIFY(zip.fileData("image-3.png").isEmpty());
        QVERIFY(!zip.fileData("book.xhtml").contains("127.0.0.1"));
    }

    void diagramBlocksAreImages_data() { // NOLINT(readability-identifier-naming): Qt Test data-provider convention.
        QTest::addColumn<int>("format");
        QTest::addColumn<QString>("noteId");
        QTest::addColumn<QString>("diagram");
        for (const auto &diagram: {QString("Mermaid"), QString("PlantUML")}) {
            for (const auto format: {EbookExporter::Format::Html, EbookExporter::Format::Epub,
                                EbookExporter::Format::Pdf, EbookExporter::Format::Chm}) {
                for (const auto &id: {QString(), QString("one")}) {
                    QTest::newRow(qPrintable(diagram + "-" + QString::number(int(format)) + (id.isEmpty() ? "-book" : "-single")))
                            << int(format) << id << diagram;
                }
            }
        }
    }

    void diagramBlocksAreImages() {
        QFETCH(int, format);
        QFETCH(QString, noteId);
        QFETCH(QString, diagram);
        if (diagram == "PlantUML" && QStandardPaths::findExecutable("plantuml").isEmpty()) QSKIP("Install PlantUML to run diagram integration test");
        const auto exportFormat = EbookExporter::Format(format);
        if (exportFormat == EbookExporter::Format::Chm && EbookExporter::chmCompiler().isEmpty()) QSKIP("Install chmcmd or hhc to run CHM integration test");
        QTemporaryDir source;
        QTemporaryDir destination;
        auto book = fixture(source.path());
        if (diagram == "Mermaid") {
            book.notes[0].content += "\n\n```MeRmAiD title\ngraph TD\nA[\"开始 & 检查\"] --> B[完成]\n```"
                                "\n\n> ```mermaid\n> sequenceDiagram\n> participant A as 用户\n> participant B as 系统\n> A->>B: 你好\n> ```"
                                "\n\n```text\nordinary-code-preserved\n```";
            book.notes[2].content += "\n\n```mermaid\ngraph LR\nC[子章节] --> D[结束]\n```";
        } else {
            book.notes[0].content += "\n\n```PlAnTuMl title\n@startuml\nAlice -> Bob: 你好\n@enduml\n```"
                                    "\n\n> ```plantuml\n> Alice -> Bob: body-only\n> ```"
                                    "\n\n```text\nordinary-code-preserved\n```";
            book.notes[2].content += "\n\n```plantuml\n@startmindmap\n* 中文\n** 子章节\n@endmindmap\n```";
        }
        QString error;
        const auto output = EbookExporter::exportBook(book, noteId, exportFormat, destination.path(), &error);
        QVERIFY2(!output.isEmpty(), qPrintable(error));
        if (qEnvironmentVariableIsSet("QSNOTE_EXPORT_ARTIFACTS")) {
            const auto directory = qEnvironmentVariable("QSNOTE_EXPORT_ARTIFACTS");
            QDir().mkpath(directory);
            QVERIFY(QFile::copy(output, directory + "/" + diagram.toLower() + "-" + (noteId.isEmpty() ? "book." : "single.") + QFileInfo(output).suffix()));
        }
        const int count = noteId.isEmpty() ? 3 : 2;
        if (exportFormat == EbookExporter::Format::Pdf) {
            QVERIFY(readFile(output).startsWith("%PDF-"));
            if (!QStandardPaths::findExecutable("pdfimages").isEmpty()) {
                QProcess process;
                process.start("pdfimages", {"-list", output});
                QVERIFY(process.waitForFinished());
                QCOMPARE(process.exitCode(), 0);
                auto images = QRegularExpression(R"((?m)^\s*\d+\s+\d+\s+image\s)").globalMatch(QString::fromUtf8(process.readAllStandardOutput()));
                int imageCount = 0;
                while (images.hasNext()) { images.next(); ++imageCount; }
                QCOMPARE(imageCount, count + 1);
            }
            if (!QStandardPaths::findExecutable("pdftotext").isEmpty()) {
                QProcess process;
                process.start("pdftotext", {output, "-"});
                QVERIFY(process.waitForFinished());
                QCOMPARE(process.exitCode(), 0);
                const auto text = process.readAllStandardOutput();
                QVERIFY(!text.contains("graph TD"));
                QVERIFY(!text.contains("sequenceDiagram"));
                QVERIFY(!text.contains("@startuml"));
                QVERIFY(!text.contains("body-only"));
                QVERIFY(text.contains("ordinary-code-preserved"));
            }
            return;
        }
        if (exportFormat == EbookExporter::Format::Chm && QStandardPaths::findExecutable("7z").isEmpty()) QSKIP("Install 7z to inspect CHM diagrams");
        auto exportedFile = [&](const QString &name) {
            if (exportFormat == EbookExporter::Format::Epub) return QZipReader(output).fileData(name);
            QProcess process;
            process.start("7z", {"x", "-so", output, name});
            if (!process.waitForFinished() || process.exitCode() != 0) return QByteArray();
            return process.readAllStandardOutput();
        };
        const auto html = exportFormat == EbookExporter::Format::Html ? readFile(output)
                        : exportedFile(exportFormat == EbookExporter::Format::Epub ? "book.xhtml" : "book.html");
        QVERIFY(!html.contains("graph TD"));
        QVERIFY(!html.contains("sequenceDiagram"));
        QVERIFY(!html.contains("@startuml"));
        QVERIFY(!html.contains("body-only"));
        QVERIFY(html.contains("ordinary-code-preserved"));
        QXmlStreamReader xml(html);
        QList<QByteArray> diagrams;
        while (!xml.atEnd()) {
            xml.readNext();
            if (!xml.isStartElement() || xml.name() != u"img" || xml.attributes().value("alt") != diagram + " 图表") continue;
            const auto src = xml.attributes().value("src").toString();
            diagrams.append(exportFormat == EbookExporter::Format::Html ? QByteArray::fromBase64(src.section(',', 1).toLatin1()) : exportedFile(src));
            if (exportFormat == EbookExporter::Format::Epub) QVERIFY(exportedFile("content.opf").contains(("href=\"" + src + "\" media-type=\"image/png\"").toUtf8()));
        }
        QVERIFY2(!xml.hasError(), qPrintable(xml.errorString()));
        QCOMPARE(diagrams.size(), count);
        for (const auto &png: diagrams) {
            const auto image = QImage::fromData(png, "PNG");
            QVERIFY(image.width() > 20 && image.height() > 20);
            bool varied = false;
            for (int y = 0; y < image.height() && !varied; ++y) {
                for (int x = 0; x < image.width() && !varied; ++x) varied = image.pixel(x, y) != image.pixel(0, 0);
            }
            QVERIFY(varied);
        }
    }

    void invalidMermaidReportsChapter() {
        QTemporaryDir source;
        QTemporaryDir destination;
        auto book = fixture(source.path());
        book.notes[0].content = "```mermaid\nthis is not a diagram\n```";
        QString error;
        QVERIFY(EbookExporter::exportBook(book, "one", EbookExporter::Format::Html, destination.path(), &error).isEmpty());
        QVERIFY(error.contains("Mermaid"));
        QVERIFY(error.contains("第一章"));
        QVERIFY(QDir(destination.path()).entryList(QDir::Files).isEmpty());
    }

    void invalidPlantUmlReportsChapter() {
        if (QStandardPaths::findExecutable("plantuml").isEmpty()) QSKIP("Install PlantUML to run diagram integration test");
        QTemporaryDir source;
        QTemporaryDir destination;
        auto book = fixture(source.path());
        book.notes[0].content = "```plantuml\n@startuml\nthis is not a diagram\n@enduml\n```";
        QString error;
        QVERIFY(EbookExporter::exportBook(book, "one", EbookExporter::Format::Html, destination.path(), &error).isEmpty());
        QVERIFY(error.contains("PlantUML"));
        QVERIFY(error.contains("第一章"));
        QVERIFY(QDir(destination.path()).entryList(QDir::Files).isEmpty());
    }

    void missingPlantUmlReportsChapter() {
        const auto path = qgetenv("PATH");
        const auto restorePath = qScopeGuard([&] { qputenv("PATH", path); });
        qputenv("PATH", QByteArray());
        QTemporaryDir source;
        QTemporaryDir destination;
        auto book = fixture(source.path());
        book.notes[0].content = "```plantuml\nAlice -> Bob\n```";
        QString error;
        QVERIFY(EbookExporter::exportBook(book, "one", EbookExporter::Format::Html, destination.path(), &error).isEmpty());
        QVERIFY(error.contains("未找到 PlantUML"));
        QVERIFY(error.contains("第一章"));
        QVERIFY(QDir(destination.path()).entryList(QDir::Files).isEmpty());
    }

    void invalidInputAndMissingAssets() {
        QTemporaryDir source;
        QTemporaryDir destination;
        auto book = fixture(source.path());
        QString error;
        QVERIFY(EbookExporter::exportBook(book, {}, EbookExporter::Format::Html, destination.path() + "/missing", &error).isEmpty());
        QVERIFY(!error.isEmpty());
        QVERIFY(EbookExporter::exportBook(book, "missing", EbookExporter::Format::Html, destination.path(), &error).isEmpty());
        QVERIFY(EbookExporter::exportBook(book, "group", EbookExporter::Format::Html, destination.path(), &error).isEmpty());
        QFile::remove(source.path() + "/图 片.png");
        QVERIFY(EbookExporter::exportBook(book, {}, EbookExporter::Format::Epub, destination.path(), &error).isEmpty());
        QVERIFY(error.contains("图片"));
        QVERIFY(QDir(destination.path()).entryList(QDir::Files).isEmpty());
        book.notes.clear();
        QVERIFY(EbookExporter::exportBook(book, {}, EbookExporter::Format::Html, destination.path(), &error).isEmpty());
        QVERIFY(error.contains("没有"));
        book = fixture(source.path());
        book.notes[0].parentId = "two";
        QVERIFY(EbookExporter::exportBook(book, {}, EbookExporter::Format::Html, destination.path(), &error).isEmpty());
        QVERIFY(error.contains("目录"));
        const auto path = qgetenv("PATH");
        qputenv("PATH", "");
        const auto output = EbookExporter::exportBook(book, {}, EbookExporter::Format::Chm, destination.path(), &error);
        qputenv("PATH", path);
        QVERIFY(output.isEmpty());
        QVERIFY(error.contains("chmcmd"));
    }
};
// NOLINTEND(readability-convert-member-functions-to-static)
QTEST_MAIN(EbookExporterTest)
#include "EbookExporterTest.moc"
