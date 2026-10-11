#include "EbookExporter.h"
#include <QBuffer>
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QImage>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPageLayout>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTextDocumentFragment>
#include <QTimer>
#include <QUuid>
#include <QWebEnginePage>
#include <QWebEngineSettings>
#include <QtGui/private/qzipwriter_p.h>
#include <cmark.h>
#include <cstdlib>
#include <memory>

namespace {
const QString style = QStringLiteral(
    "body{font-family:sans-serif;color:#24292f;line-height:1.7;max-width:900px;margin:32px auto;padding:0 24px;}"
    "a{color:#2864b4;}pre{background:#f6f8fa;padding:16px;white-space:pre-wrap;overflow-wrap:anywhere;}"
    "code{font-family:monospace;}img{max-width:100%;height:auto;}table{border-collapse:collapse;}"
    "td,th{border:1px solid #ccc;padding:6px 12px;}blockquote{border-left:3px solid #ccc;padding-left:16px;}"
    ".chapter{page-break-before:always;}h1,h2,h3{page-break-after:avoid;}"
    "@media print{body{margin:0;max-width:none;padding:0;}a{color:inherit;text-decoration:none;}}");

QString page(const QString &title, const QString &body) {
    return "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<!DOCTYPE html>\n"
           "<html xmlns=\"http://www.w3.org/1999/xhtml\" xmlns:epub=\"http://www.idpf.org/2007/ops\" lang=\"zh-CN\" xml:lang=\"zh-CN\">"
           "<head><meta charset=\"utf-8\"/><title>" + title.toHtmlEscaped() + "</title><style>" + style + "</style></head><body>" + body + "</body></html>";
}

bool writeFile(const QString &path, const QByteArray &data, QString *error) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.flush()) {
        *error = "无法写入 " + path + "：" + file.errorString();
        return false;
    }
    return true;
}

struct Chapter {
    const Note *note;
    int depth;
    QString anchor;
};

QString contents(const QList<Chapter> &chapters, bool chm) {
    QString result;
    int depth = -1;
    for (const auto &chapter: chapters) {
        const bool child = depth < chapter.depth;
        while (depth < chapter.depth) {
            result += chm ? "<ul>" : "<ol>";
            ++depth;
        }
        while (depth > chapter.depth) {
            result += chm ? "</li></ul>" : "</li></ol>";
            --depth;
        }
        if (!child) result += "</li>";
        const auto target = "book.html#" + chapter.anchor;
        if (chm) {
            result += R"(<li><object type="text/sitemap"><param name="Name" value=")" + chapter.note->title.toHtmlEscaped()
                    + R"("><param name="Local" value=")" + target + "\"></object>";
        } else {
            result += "<li><a href=\"" + target + "\">" + chapter.note->title.toHtmlEscaped() + "</a>";
        }
    }
    while (depth-- >= 0) result += chm ? "</li></ul>" : "</li></ol>";
    return result;
}

// EPUB 2 NCX：xreader/Evince 等阅读器的侧边索引只认它。
QString ncx(const QString &title, const QString &uid, const QList<Chapter> &chapters) {
    int maxDepth = 0;
    for (const auto &chapter: chapters) maxDepth = qMax(maxDepth, chapter.depth + 1);
    QString result = R"(<?xml version="1.0" encoding="utf-8"?><ncx xmlns="http://www.daisy.org/z3986/2005/ncx/" version="2005-1"><head><meta name="dtb:uid" content=")"
            + uid + R"("/><meta name="dtb:depth" content=")" + QString::number(maxDepth)
            + R"("/><meta name="dtb:totalPageCount" content="0"/><meta name="dtb:maxPageNumber" content="0"/></head><docTitle><text>)"
            + title.toHtmlEscaped() + "</text></docTitle><navMap>";
    int depth = -1;
    for (int i = 0; i < chapters.size(); ++i) {
        const auto &chapter = chapters[i];
        const bool child = depth < chapter.depth;
        while (depth < chapter.depth) ++depth;
        while (depth > chapter.depth) { result += "</navPoint>"; --depth; }
        if (!child) result += "</navPoint>";
        result += "<navPoint id=\"" + chapter.anchor + "\" playOrder=\"" + QString::number(i + 1) + "\"><navLabel><text>"
                + chapter.note->title.toHtmlEscaped() + "</text></navLabel><content src=\"book.xhtml#" + chapter.anchor + "\"/>";
    }
    while (depth-- >= 0) result += "</navPoint>";
    return result + "</navMap></ncx>";
}

QString slug(QString text) {
    text = text.toLower().trimmed();
    text.remove(QRegularExpression(R"([^\p{L}\p{N}_\-\s])"));
    return text.replace(QRegularExpression("\\s+"), "-");
}

QByteArray imageData(const QUrl &url, QString *error) {
    if (url.scheme() == "data") {
        const auto encoded = url.toString(QUrl::FullyEncoded).toLatin1();
        const auto comma = encoded.indexOf(',');
        return encoded.left(comma).endsWith(";base64") ? QByteArray::fromBase64(encoded.mid(comma + 1))
                                                       : QByteArray::fromPercentEncoding(encoded.mid(comma + 1));
    }
    if (url.isLocalFile()) {
        QFile file(url.toLocalFile());
        if (file.open(QIODevice::ReadOnly)) return file.readAll();
        *error = file.errorString();
        return {};
    }
    if (url.scheme() != "https" && url.scheme() != "http") {
        *error = "不支持的图片地址";
        return {};
    }
    QNetworkAccessManager network;
    QNetworkRequest request(url);
    request.setTransferTimeout(30000);
    auto *reply = network.get(request);
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, reply, &QNetworkReply::abort);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timer.start(30000);
    loop.exec(QEventLoop::ExcludeUserInputEvents);
    if (reply->error() != QNetworkReply::NoError) {
        *error = reply->errorString();
        return {};
    }
    return reply->readAll();
}

bool addPdfBookmarks(const QString &path, const QList<Chapter> &chapters, QString *error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadWrite)) { *error = file.errorString(); return false; }
    const auto pdf = file.readAll();
    // Only consume the classic xref table emitted by Qt WebEngine/Skia, not arbitrary PDFs.
    // Append an incremental update so page streams, links, fonts and images stay intact.
    *error = "无法为 PDF 添加书签：生成的 PDF 结构不受支持。";
    const auto start = QRegularExpression(R"(startxref\s+(\d+)\s+%%EOF\s*$)").match(QString::fromLatin1(pdf.right(100)));
    if (!start.hasMatch()) return false;
    const auto previousXref = start.captured(1).toLongLong();
    if (previousXref < 0 || previousXref >= pdf.size()) return false;
    const auto xref = QString::fromLatin1(pdf.mid(previousXref));
    const auto header = QRegularExpression(R"(^xref\s+0 (\d+)\s*\n)").match(xref);
    const auto trailer = QRegularExpression(R"(trailer\s*<<(.*?)>>\s*startxref)", QRegularExpression::DotMatchesEverythingOption).match(xref);
    if (!header.hasMatch() || !trailer.hasMatch()) return false;
    auto trailerFields = trailer.captured(1);
    const QRegularExpression sizePattern(R"(/Size\s+(\d+)\b)");
    const QRegularExpression rootPattern(R"(/Root\s+(\d+)\s+0\s+R\b)");
    const auto size = sizePattern.match(trailerFields);
    const auto root = rootPattern.match(trailerFields);
    if (!size.hasMatch() || !root.hasMatch() || trailerFields.contains("/Prev") || trailerFields.contains("/Encrypt")) return false;
    const int firstId = size.captured(1).toInt();
    const int rootId = root.captured(1).toInt();
    const auto entries = xref.mid(header.capturedEnd(), trailer.capturedStart() - header.capturedEnd()).trimmed().split('\n');
    if (firstId != header.captured(1).toInt() || entries.size() != firstId || rootId <= 0 || rootId >= firstId) return false;
    const auto rootEntry = QRegularExpression(R"(^(\d{10}) 00000 n\s*$)").match(entries[rootId]);
    if (!rootEntry.hasMatch()) return false;
    const auto rootOffset = rootEntry.captured(1).toLongLong();
    if (rootOffset < 0 || rootOffset >= previousXref) return false;
    const auto rootEnd = pdf.indexOf("endobj", rootOffset);
    if (rootEnd < 0 || rootEnd >= previousXref) return false;
    const auto catalogObject = pdf.mid(rootOffset, rootEnd - rootOffset).trimmed();
    if (!catalogObject.startsWith(QByteArray::number(rootId) + " 0 obj") || !catalogObject.endsWith(">>")) return false;
    auto catalog = catalogObject.mid(catalogObject.indexOf("<<"));
    if (!catalog.contains("/Type /Catalog") || !catalog.contains("/Dests ") || catalog.contains("/Outlines") || catalog.contains("/PageMode")) return false;

    const int outlineId = firstId + 1;
    auto reference = [](int id) { return QByteArray::number(id) + " 0 R"; };
    // Index 0 is the outline root; chapter indices follow in sidebar traversal order.
    QList<QList<int>> children(chapters.size() + 1);
    QList<int> parents{0};
    for (int i = 0; i < chapters.size(); ++i) {
        while (parents.size() > chapters[i].depth + 1) parents.removeLast();
        children[parents.last()].append(i + 1);
        parents.append(i + 1);
    }
    QList<QByteArray> objects;
    catalog.chop(2);
    objects.append(catalog + "\n/Outlines " + reference(outlineId) + "\n/PageMode /UseOutlines\n>>");
    objects.append("<< /Type /Outlines\n/Count " + QByteArray::number(chapters.size())
                   + "\n/First " + reference(outlineId + children[0].first())
                   + "\n/Last " + reference(outlineId + children[0].last()) + "\n>>");
    for (const auto &chapter: chapters) {
        QByteArray title = QByteArray::fromHex("feff");
        for (const auto character: chapter.note->title) {
            title.append(char(character.unicode() >> 8));
            title.append(char(character.unicode() & 0xff));
        }
        objects.append("<< /Title <" + title.toHex() + ">\n/Dest /" + chapter.anchor.toLatin1());
    }
    for (int parent = 0; parent < children.size(); ++parent) {
        const auto &siblings = children[parent];
        for (int i = 0; i < siblings.size(); ++i) {
            const int child = siblings[i];
            auto &object = objects[child + 1];
            object += "\n/Parent " + reference(outlineId + parent);
            if (i > 0) object += "\n/Prev " + reference(outlineId + siblings[i - 1]);
            if (i + 1 < siblings.size()) object += "\n/Next " + reference(outlineId + siblings[i + 1]);
            if (!children[child].isEmpty()) {
                int end = child;
                while (end < chapters.size() && chapters[end].depth > chapters[child - 1].depth) ++end;
                object += "\n/First " + reference(outlineId + children[child].first())
                        + "\n/Last " + reference(outlineId + children[child].last())
                        + "\n/Count " + QByteArray::number(end - child);
            }
            object += "\n>>";
        }
    }
    QByteArray update = "\n";
    QByteArray offsets;
    for (int i = 0; i < objects.size(); ++i) {
        offsets += QByteArray::number(pdf.size() + update.size()).rightJustified(10, '0') + " 00000 n \n";
        update += QByteArray::number(firstId + i) + " 0 obj\n" + objects[i] + "\nendobj\n";
    }
    const auto newXref = pdf.size() + update.size();
    trailerFields.replace(sizePattern, "/Size " + QString::number(firstId + objects.size()));
    trailerFields.replace(rootPattern, "/Root " + QString::fromLatin1(reference(firstId)));
    update += "xref\n0 1\n0000000000 65535 f \n" + QByteArray::number(firstId) + " " + QByteArray::number(objects.size()) + "\n" + offsets
            + "trailer\n<<" + trailerFields.toLatin1() + "\n/Prev " + QByteArray::number(previousXref)
            + ">>\nstartxref\n" + QByteArray::number(newXref) + "\n%%EOF\n";
    if (file.write(update) != update.size() || !file.flush()) { *error = "无法写入 PDF 书签：" + file.errorString(); return false; }
    error->clear();
    return true;
}

bool printPdf(const QString &htmlPath, const QString &pdfPath, const QList<Chapter> &chapters, QString *error) {
    QEventLoop loop;
    QTimer timeout;
    QWebEnginePage browser;
    browser.settings()->setAttribute(QWebEngineSettings::JavascriptEnabled, false);
    browser.settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, false);
    bool success = false;
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(&browser, &QWebEnginePage::loadFinished, &loop, [&](bool ok) {
        if (!ok) { loop.quit(); return; }
        browser.printToPdf(pdfPath, QPageLayout(QPageSize(QPageSize::A4), QPageLayout::Portrait, QMarginsF(15, 15, 15, 15)));
    });
    QObject::connect(&browser, &QWebEnginePage::pdfPrintingFinished, &loop, [&](const QString &, bool ok) {
        success = ok;
        loop.quit();
    });
    timeout.start(60000);
    browser.load(QUrl::fromLocalFile(htmlPath));
    loop.exec(QEventLoop::ExcludeUserInputEvents);
    if (!success) *error = "PDF 生成失败或超时。";
    return success && addPdfBookmarks(pdfPath, chapters, error);
}
}// namespace

QString EbookExporter::chmCompiler() {
    auto compiler = QStandardPaths::findExecutable("chmcmd");
#ifdef Q_OS_WIN
    if (compiler.isEmpty()) compiler = QStandardPaths::findExecutable("hhc");
#endif
    return compiler;
}

QString EbookExporter::exportBook(const Notebook &book, const QString &noteId, Format format,
                                 const QString &directory, QString *error) {
    error->clear();
    if (!QFileInfo(directory).isDir()) {
        *error = "请选择已有的保存目录。";
        return {};
    }
    if (format == Format::Chm && chmCompiler().isEmpty()) {
        *error = "CHM 导出需要 chmcmd（Free Pascal 的 fp-utils）；Windows 也可安装 HTML Help Workshop 并将 hhc 加入 PATH。";
        return {};
    }
    QList<Chapter> chapters;
    if (!noteId.isEmpty()) {
        for (const auto &note: book.notes) {
            if (note.id == noteId && !note.folder) chapters.append({&note, 0, "chapter-1"});
        }
    } else {
        QList<QPair<QString, int>> pending{{QString(), 0}};
        QSet<QString> visited;
        while (!pending.isEmpty()) {
            const auto entry = pending.takeLast();
            if (!entry.first.isEmpty()) {
                if (visited.contains(entry.first)) continue;
                visited.insert(entry.first);
                for (const auto &note: book.notes) {
                    if (note.id == entry.first) chapters.append({&note, entry.second - 1, "chapter-" + QString::number(chapters.size() + 1)});
                }
            }
            for (auto it = book.notes.crbegin(); it != book.notes.crend(); ++it) {
                if (it->parentId == entry.first) pending.append({it->id, entry.second + 1});
            }
        }
        if (visited.size() != book.notes.size()) {
            *error = "章节目录包含无效的父章节或循环，请先修复目录。";
            return {};
        }
    }
    if (chapters.isEmpty()) {
        *error = noteId.isEmpty() ? "电子书中没有可导出的章节。" : "请选择当前电子书中的文档。";
        return {};
    }
    const auto title = noteId.isEmpty() ? book.name : chapters.first().note->title;
    QTemporaryDir staging;
    if (!staging.isValid()) { *error = "无法创建导出临时目录。"; return {}; }
    QMap<QString, QString> chapterPaths;
    for (const auto &chapter: chapters) {
        if (!chapter.note->path.isEmpty()) chapterPaths[QDir::cleanPath(QDir(book.path).absoluteFilePath(chapter.note->path))] = chapter.anchor;
    }
    QMap<QString, QByteArray> assets;
    QMap<QString, QString> imagePaths;
    QString body = "<h1>" + title.toHtmlEscaped() + "</h1><nav epub:type=\"toc\"><h2>目录</h2>" + contents(chapters, false) + "</nav>";
    body.replace("book.html#", "#");
    for (const auto &chapter: chapters) {
        const auto &note = *chapter.note;
        const auto source = QDir(book.path).absoluteFilePath(note.path.isEmpty() ? "document.md" : note.path);
        const auto base = QUrl::fromLocalFile(source);
        const auto utf8 = note.folder ? QByteArray() : note.content.toUtf8();
        const std::unique_ptr<cmark_node, decltype(&cmark_node_free)> root(cmark_parse_document(utf8.constData(), size_t(utf8.size()), CMARK_OPT_DEFAULT), cmark_node_free);
        const std::unique_ptr<cmark_iter, decltype(&cmark_iter_free)> iter(cmark_iter_new(root.get()), cmark_iter_free);
        QList<cmark_node *> excludedLinks;
        while (cmark_iter_next(iter.get()) != CMARK_EVENT_DONE) {
            if (cmark_iter_get_event_type(iter.get()) != CMARK_EVENT_ENTER) continue;
            auto *node = cmark_iter_get_node(iter.get());
            const auto type = cmark_node_get_type(node);
            if (type != CMARK_NODE_IMAGE && type != CMARK_NODE_LINK) continue;
            const QUrl original(QString::fromUtf8(cmark_node_get_url(node)));
            const auto resolved = base.resolved(original);
            QString replacement;
            if (type == CMARK_NODE_IMAGE) {
                const auto key = resolved.toString();
                if (!imagePaths.contains(key)) {
                    const auto bytes = imageData(resolved, error);
                    const auto image = QImage::fromData(bytes);
                    if (image.isNull()) {
                        *error = "无法读取图片 " + original.toString() + "（" + note.title + "）：" + *error;
                        return {};
                    }
                    QByteArray png;
                    QBuffer buffer(&png);
                    buffer.open(QIODevice::WriteOnly);
                    image.save(&buffer, "PNG");
                    const auto name = "image-" + QString::number(assets.size() + 1) + ".png";
                    assets[name] = png;
                    imagePaths[key] = name;
                }
                replacement = imagePaths[key];
            } else if (resolved.isLocalFile()) {
                const auto target = chapterPaths.value(QDir::cleanPath(resolved.toLocalFile()));
                if (!target.isEmpty()) replacement = "#" + target + (original.hasFragment() ? "-" + original.fragment() : QString());
                else if (original.path().isEmpty() && original.hasFragment()) replacement = "#" + chapter.anchor + "-" + original.fragment();
                else {
                    // A single-document export cannot link to chapters outside its scope.
                    excludedLinks.append(node);
                }
            }
            if (!replacement.isEmpty()) cmark_node_set_url(node, replacement.toUtf8().constData());
        }
        for (auto *link: excludedLinks) {
            while (auto *child = cmark_node_first_child(link)) cmark_node_insert_before(link, child);
            cmark_node_unlink(link);
            cmark_node_free(link);
        }
        auto *rendered = cmark_render_html(root.get(), CMARK_OPT_DEFAULT);
        QString html = QString::fromUtf8(rendered);
        std::free(rendered);
        // Give headings GitBook-style slugs, scoped to their chapter to avoid collisions.
        const QRegularExpression headings("<h([1-6])>(.*?)</h[1-6]>", QRegularExpression::DotMatchesEverythingOption);
        auto matches = headings.globalMatch(html);
        QList<QRegularExpressionMatch> headingMatches;
        while (matches.hasNext()) headingMatches.append(matches.next());
        QMap<QString, int> slugs;
        qsizetype offset = 0;
        for (const auto &match: headingMatches) {
            const auto text = QTextDocumentFragment::fromHtml(match.captured(2)).toPlainText();
            auto id = slug(text);
            const auto duplicate = slugs[id]++;
            if (duplicate) id += "-" + QString::number(duplicate);
            const auto start = "<h" + match.captured(1) + " id=\"" + chapter.anchor + "-" + id.toHtmlEscaped() + "\">";
            const int length = 4;
            html.replace(match.capturedStart() + offset, length, start);
            offset += start.size() - length;
        }
        html.replace(" data-footnotes>", " data-footnotes=\"\">");
        html.replace("<eq ", "<code ").replace("</eq>", "</code>");
        html.replace("<eqn ", "<pre ").replace("</eqn>", "</pre>");
        html.replace("id=\"fn", "id=\"" + chapter.anchor + "-fn");
        html.replace("href=\"#fn", "href=\"#" + chapter.anchor + "-fn");
        body += R"(<div class="chapter" id=")" + chapter.anchor + "\"><h1>" + note.title.toHtmlEscaped() + "</h1>" + html + "</div>";
    }
    auto html = page(title, body);
    QString extension;
    switch (format) {
    case Format::Epub: extension = "epub"; break;
    case Format::Pdf: extension = "pdf"; break;
    case Format::Chm: extension = "chm"; break;
    case Format::Html: extension = "html"; break;
    }
    const auto output = staging.filePath("output." + extension);
    if (format == Format::Html) {
        for (auto it = assets.cbegin(); it != assets.cend(); ++it) html.replace("src=\"" + it.key() + "\"", "src=\"data:image/png;base64," + QString::fromLatin1(it.value().toBase64()) + "\"");
        if (!writeFile(output, html.toUtf8(), error)) return {};
    } else if (format == Format::Epub) {
        QZipWriter zip(output);
        zip.setCompressionPolicy(QZipWriter::NeverCompress);
        zip.addFile("mimetype", QByteArray("application/epub+zip"));
        zip.setCompressionPolicy(QZipWriter::AutoCompress);
        zip.addFile("META-INF/container.xml", QByteArray(R"(<?xml version="1.0"?><container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container"><rootfiles><rootfile full-path="content.opf" media-type="application/oebps-package+xml"/></rootfiles></container>)"));
        zip.addFile("book.xhtml", html.toUtf8());
        auto toc = contents(chapters, false);
        toc.replace("book.html#", "book.xhtml#");
        zip.addFile("nav.xhtml", page(title, "<nav epub:type=\"toc\" id=\"toc\"><h1>目录</h1>" + toc + "</nav>").toUtf8());
        const auto uid = "urn:uuid:" + QUuid::createUuid().toString(QUuid::WithoutBraces);
        zip.addFile("toc.ncx", ncx(title, uid, chapters).toUtf8());
        QString manifest = R"(<item id="book" href="book.xhtml" media-type="application/xhtml+xml"/><item id="nav" href="nav.xhtml" media-type="application/xhtml+xml" properties="nav"/><item id="ncx" href="toc.ncx" media-type="application/x-dtbncx+xml"/>)";
        for (auto it = assets.cbegin(); it != assets.cend(); ++it) {
            zip.addFile(it.key(), it.value());
            manifest += "<item id=\"" + it.key() + "\" href=\"" + it.key() + R"(" media-type="image/png"/>)";
        }
        const auto opf = R"(<?xml version="1.0" encoding="utf-8"?><package xmlns="http://www.idpf.org/2007/opf" version="3.0" unique-identifier="id"><metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:identifier id="id">)"
                + uid + "</dc:identifier><dc:title>" + title.toHtmlEscaped()
                + "</dc:title><dc:language>zh-CN</dc:language><meta property=\"dcterms:modified\">" + QDateTime::currentDateTimeUtc().toString("yyyy-MM-dd'T'HH:mm:ss'Z'")
                + "</meta></metadata><manifest>" + manifest + R"(</manifest><spine toc="ncx"><itemref idref="book"/></spine></package>)";
        zip.addFile("content.opf", opf.toUtf8());
        zip.close();
        if (zip.status() != QZipWriter::NoError) { *error = "无法写入 EPUB 文件。"; return {}; }
    } else {
        if (!writeFile(staging.filePath("book.html"), html.toUtf8(), error)) return {};
        for (auto it = assets.cbegin(); it != assets.cend(); ++it) {
            if (!writeFile(staging.filePath(it.key()), it.value(), error)) return {};
        }
        if (format == Format::Pdf) {
            if (!printPdf(staging.filePath("book.html"), output, chapters, error)) return {};
        } else {
            const auto toc = "<!DOCTYPE HTML><html><head><meta charset=\"utf-8\"></head><body>" + contents(chapters, true) + "</body></html>";
            // Numeric entities keep Chinese TOC labels portable across compiler code pages.
            QString asciiToc;
            for (const auto character: toc.toUcs4()) asciiToc += character > 127 ? "&#" + QString::number(character) + ";" : QString(QChar(ushort(character)));
            const auto project = "[OPTIONS]\nCompatibility=1.1 or later\nCompiled file=output.chm\nContents file=contents.hhc\nDefault topic=book.html\nTitle=QSNote\nLanguage=0x804 Chinese (PRC)\n[FILES]\nbook.html\n" + assets.keys().join('\n') + "\n";
            if (!writeFile(staging.filePath("contents.hhc"), asciiToc.toUtf8(), error) || !writeFile(staging.filePath("book.hhp"), project.toUtf8(), error)) return {};
            QProcess compiler;
            compiler.setWorkingDirectory(staging.path());
            compiler.setProcessChannelMode(QProcess::MergedChannels);
            QEventLoop loop;
            QTimer timer;
            QObject::connect(&compiler, &QProcess::finished, &loop, &QEventLoop::quit);
            QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
            compiler.start(chmCompiler(), {"book.hhp"});
            if (!compiler.waitForStarted()) { *error = compiler.errorString(); return {}; }
            timer.start(60000);
            loop.exec(QEventLoop::ExcludeUserInputEvents);
            if (compiler.state() != QProcess::NotRunning) { compiler.kill(); compiler.waitForFinished(); }
            QFile result(output);
            // hhc returns 1 on success; require a real CHM header as well as normal exit.
            const bool exitOk = compiler.exitCode() == 0 || (QFileInfo(compiler.program()).baseName().compare("hhc", Qt::CaseInsensitive) == 0 && compiler.exitCode() == 1);
            if (compiler.exitStatus() != QProcess::NormalExit || !exitOk || !result.open(QIODevice::ReadOnly) || result.read(4) != "ITSF") {
                *error = "CHM 编译失败：" + QString::fromLocal8Bit(compiler.readAll()).right(2000);
                return {};
            }
        }
    }
    const auto baseName = NotebookStore::sanitized(title);
    auto destination = QDir(directory).filePath(baseName + "." + extension);
    for (int suffix = 2; QFileInfo::exists(destination); ++suffix) destination = QDir(directory).filePath(baseName + "-" + QString::number(suffix) + "." + extension);
    QFile generated(output);
    if (!generated.copy(destination)) { *error = "无法保存导出文件：" + generated.errorString(); return {}; }
    return destination;
}
