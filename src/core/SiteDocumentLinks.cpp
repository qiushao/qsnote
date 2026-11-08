#include "SiteDocuments.h"
#include "AtomicFile.h"
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QMap>
#include <QUrl>
#include <cmark.h>
#include <algorithm>
#include <vtextedit/markdownutils.h>

namespace {
struct Destination {
    int start;
    int end;
    QString url;
};

struct LinkSource {
    QByteArray bytes;
    QList<int> lines;
    QList<Destination> destinations;
};

QString cleanUrl(const QByteArray &raw) {
    const auto markdown = "[link](" + raw + ')';
    auto *document = cmark_parse_document(markdown.constData(), size_t(markdown.size()), CMARK_OPT_DEFAULT);
    auto *paragraph = cmark_node_first_child(document);
    auto *link = paragraph ? cmark_node_first_child(paragraph) : nullptr;
    const auto result = link && cmark_node_get_type(link) == CMARK_NODE_LINK ? QString::fromUtf8(cmark_node_get_url(link)) : QString();
    cmark_node_free(document);
    return result;
}

int coordinate(cmark_node *node, const LinkSource &source, int line, int column) {
    auto *paragraph = cmark_node_parent(node);
    while (paragraph && cmark_node_get_type(paragraph) != CMARK_NODE_PARAGRAPH) paragraph = cmark_node_parent(paragraph);
    if (paragraph && line > cmark_node_get_start_line(paragraph)) {
        const int block = cmark_node_get_start_column(paragraph) - 1;
        const auto start = source.lines.value(line - 1);
        int width = 0, markers = 0;
        while (start + width < source.bytes.size()) {
            const auto character = source.bytes[start + width];
            if (character == ' ') ++width;
            else if (character == '>' && width <= block) {
                ++width;
                if (start + width < source.bytes.size() && source.bytes[start + width] == ' ') ++width;
                markers = width;
            } else break;
        }
        column += (width < block ? markers : width) - block;
    }
    return source.lines.value(line - 1) + qMax(1, column) - 1;
}

void reference(void *data, int line, const char *text, int length, int start, int end) {
    auto &source = *static_cast<LinkSource *>(data);
    const auto base = source.lines.value(line - 1);
    const auto prefix = source.bytes.indexOf(QByteArray(text, length), base);
    if (prefix < base || (source.bytes.indexOf('\n', base) >= 0 && prefix > source.bytes.indexOf('\n', base))) return;
    source.destinations.append({int(prefix) + start, int(prefix) + end, cleanUrl(QByteArray(text + start, end - start))});
}

QString relocated(const QString &path, const QMap<QString, QString> &files, const QMap<QString, QString> &folders) {
    if (files.contains(path)) return files.value(path);
    QString best, target;
    for (auto it = folders.cbegin(); it != folders.cend(); ++it) {
        if (path.startsWith(it.key() + '/') && it.key().size() > best.size()) {
            best = it.key();
            target = it.value();
        }
    }
    return best.isEmpty() ? path : target + path.mid(best.size());
}
}

bool SiteDocuments::updateLinks(Notebook &book, QString *error) {
    QMap<QString, QString> files, folders;
    const QDir root(book.path);
    const auto roots = collectionMoves(book);
    for (auto it = roots.cbegin(); it != roots.cend(); ++it) folders.insert(root.absoluteFilePath(it.key()), root.absoluteFilePath(it.value()));
    for (const auto &note: book.notes) {
        if (!isDocument(note) || note.path.isEmpty()) continue;
        const auto path = sourcePath(book, note);
        if (path == note.path) continue;
        (note.folder ? folders : files).insert(root.absoluteFilePath(note.path), root.absoluteFilePath(path));
    }
    if (files.isEmpty() && folders.isEmpty()) return true;
    QList<Note> external;
    QSet<QString> indexed;
    for (const auto &note: book.notes) indexed.insert(root.absoluteFilePath(note.path));
    QDirIterator sourceFiles(book.path + "/source", {"*.md", "*.markdown"}, QDir::Files, QDirIterator::Subdirectories);
    while (sourceFiles.hasNext()) {
        const auto path = sourceFiles.next();
        if (indexed.contains(path) || path.startsWith(book.path + "/source/docs/")) continue;
        Note note;
        note.path = root.relativeFilePath(path);
        if (!NotebookStore::readFile(note, path, error)) return false;
        external.append(note);
    }
    QList<Note *> notes;
    for (auto &note: book.notes) notes.append(&note);
    for (auto &note: external) notes.append(&note);
    for (auto *entry: notes) {
        auto &note = *entry;
        if (note.folder || note.path.isEmpty()) continue;
        const auto before = root.absoluteFilePath(note.path);
        const auto after = relocated(before, files, folders);
        LinkSource source{note.content.toUtf8(), {0}, {}};
        for (int i = 0; i < source.bytes.size(); ++i) if (source.bytes[i] == '\n') source.lines.append(i + 1);
        auto *parser = cmark_parser_new(CMARK_OPT_DEFAULT);
        cmark_parser_set_reference_destination_callback(parser, reference, &source);
        cmark_parser_feed(parser, source.bytes.constData(), size_t(source.bytes.size()));
        auto *document = cmark_parser_finish(parser);
        auto *iterator = cmark_iter_new(document);
        while (cmark_iter_next(iterator) != CMARK_EVENT_DONE) {
            auto *node = cmark_iter_get_node(iterator);
            if (cmark_iter_get_event_type(iterator) != CMARK_EVENT_ENTER) continue;
            if (cmark_node_get_type(node) != CMARK_NODE_LINK && cmark_node_get_type(node) != CMARK_NODE_IMAGE) continue;
            const auto line = cmark_node_get_url_start_line(node), endLine = cmark_node_get_url_end_line(node);
            if (line <= 0 || endLine <= 0) continue;
            const auto start = coordinate(node, source, line, cmark_node_get_url_start_column(node));
            const auto end = coordinate(node, source, endLine, cmark_node_get_url_end_column(node)) + 1;
            source.destinations.append({start, end, QString::fromUtf8(cmark_node_get_url(node))});
        }
        cmark_iter_free(iterator);
        cmark_node_free(document);
        cmark_parser_free(parser);
        const auto images = vte::MarkdownUtils::fetchImageLinks(note.content, QFileInfo(before).path(), vte::MarkdownLink::LocalRelativeInternal | vte::MarkdownLink::LocalRelativeExternal);
        for (const auto &image: images) {
            if (image.m_syntax != vte::MarkdownLink::Syntax::Html || !image.hasUrlSpan()) continue;
            source.destinations.append({int(note.content.left(image.m_urlStart).toUtf8().size()), int(note.content.left(image.m_urlEnd).toUtf8().size()), image.m_urlInLink});
        }
        std::sort(source.destinations.begin(), source.destinations.end(), [](const Destination &a, const Destination &b) { return a.start > b.start; });
        for (const auto &destination: source.destinations) {
            QUrl url(destination.url);
            if (!url.isRelative() || url.path().isEmpty() || url.path().startsWith('/')) continue;
            const auto oldTarget = QDir::cleanPath(QFileInfo(before).dir().absoluteFilePath(url.path()));
            const auto target = relocated(oldTarget, files, folders);
            if (oldTarget == target && QFileInfo(before).path() == QFileInfo(after).path()) continue;
            url.setPath(QFileInfo(after).dir().relativeFilePath(target));
            source.bytes.replace(destination.start, destination.end - destination.start, url.toEncoded());
        }
        const auto content = QString::fromUtf8(source.bytes);
        if (content != note.content) {
            note.content = content;
            note.contentDirty = true;
        }
    }
    for (const auto &note: external) {
        if (!note.contentDirty) continue;
        QByteArray bytes;
        if (!NotebookStore::fileContents(note, bytes, error) || !AtomicFile::write(root.filePath(note.path), bytes, error)) return false;
    }
    return true;
}
