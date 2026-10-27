#include "SourceHighlighter.h"
#include "core/SourceLanguage.h"
#include <Definition>
#include <Format>
#include <QDebug>
#include <QFile>
#include <QTextBlock>
#include <QTextDocument>
#include <Repository>
#include <Theme>

namespace {
QByteArray querySource(const QString &name) {
    QFile file(":/syntax/" + name + ".scm");
    if (!file.open(QIODevice::ReadOnly)) return {};
    return file.readAll();
}
}// namespace

SourceHighlighter::SourceHighlighter(QTextDocument *document, const QString &fileName, bool dark)
    : KSyntaxHighlighting::SyntaxHighlighter(document), dark_(dark) {
    static KSyntaxHighlighting::Repository repository;
    if (repository.customSearchPaths().isEmpty()) repository.addCustomSearchPath(":/demo/data");
    setTheme(repository.defaultTheme(dark ? KSyntaxHighlighting::Repository::DarkTheme : KSyntaxHighlighting::Repository::LightTheme));
    const auto language = SourceLanguage::forFile(fileName);
    auto definition = repository.definitionForFileName(fileName);
    if (!definition.isValid() && language.grammar) {
        static const QHash<QString, QString> extensions = {{"python", "py"}, {"javascript", "js"}, {"typescript", "ts"}, {"bash", "sh"}};
        definition = repository.definitionForFileName("source." + extensions.value(language.name, language.name));
    }
    setDefinition(definition);

    if (!language.grammar) return;
    auto source = querySource(language.name);
    if (language.name == "cpp") source.prepend(querySource("c") + '\n');
    if (language.name == "typescript" || language.name == "tsx") source.prepend(querySource("javascript") + '\n');
    uint32_t errorOffset = 0;
    TSQueryError error = TSQueryErrorNone;
    query_.reset(ts_query_new(language.grammar, source.constData(), static_cast<uint32_t>(source.size()), &errorOffset, &error));
    if (!query_ || source.isEmpty()) {
        qWarning() << "Cannot load source highlighting query" << language.name << errorOffset << error;
        return;
    }
    parser_.reset(ts_parser_new());
    if (!ts_parser_set_language(parser_.get(), language.grammar)) {
        parser_.reset();
        return;
    }
    timer_.setSingleShot(true);
    timer_.setInterval(100);
    connect(&timer_, &QTimer::timeout, this, &SourceHighlighter::updateSymbols);
    connect(document, &QTextDocument::contentsChange, this, [this](int, int removed, int added) {
        if (removed == 0 && added == 0) return;
        spans_.clear();
        timer_.start();
    });
    timer_.start();
}

QColor SourceHighlighter::colorFor(const QByteArray &capture) const {
    if (capture == "type") return {dark_ ? "#4ec9b0" : "#267f99"};
    if (capture == "function") return {dark_ ? "#dcdcaa" : "#795e26"};
    if (capture == "property") return {dark_ ? "#9cdcfe" : "#0057ae"};
    return {dark_ ? "#c586c0" : "#af00db"};
}

void SourceHighlighter::updateSymbols() {
    // Parse one snapshot after a burst of edits, never once per highlighted line.
    const auto source = document()->toPlainText();
    const auto encoding = Q_BYTE_ORDER == Q_LITTLE_ENDIAN ? TSInputEncodingUTF16LE : TSInputEncodingUTF16BE;
    const std::unique_ptr<TSTree, decltype(&ts_tree_delete)> tree(
            ts_parser_parse_string_encoding(parser_.get(), nullptr, reinterpret_cast<const char *>(source.utf16()), static_cast<uint32_t>(source.size() * sizeof(char16_t)), encoding), ts_tree_delete);
    spans_.clear();
    if (tree) {
        const std::unique_ptr<TSQueryCursor, decltype(&ts_query_cursor_delete)> cursor(ts_query_cursor_new(), ts_query_cursor_delete);
        ts_query_cursor_exec(cursor.get(), query_.get(), ts_tree_root_node(tree.get()));
        TSQueryMatch match;
        uint32_t index = 0;
        while (ts_query_cursor_next_capture(cursor.get(), &match, &index)) {
            const auto &capture = match.captures[index];
            const auto start = ts_node_start_point(capture.node);
            const auto end = ts_node_end_point(capture.node);
            if (start.row != end.row) continue;
            uint32_t length = 0;
            const auto *name = ts_query_capture_name_for_id(query_.get(), capture.index, &length);
            spans_[static_cast<int>(start.row)].append({static_cast<int>(start.column / sizeof(char16_t)),
                                                        static_cast<int>((end.column - start.column) / sizeof(char16_t)),
                                                        colorFor(QByteArray(name, static_cast<int>(length)))});
        }
    }
    rehighlight();
}

void SourceHighlighter::highlightBlock(const QString &text) {
    if (definition().isValid()) KSyntaxHighlighting::SyntaxHighlighter::highlightBlock(text);
    const auto spans = spans_.constFind(currentBlock().blockNumber());
    if (spans == spans_.cend()) return;
    for (const auto &span: *spans) {
        auto style = format(span.start);
        style.setForeground(span.color);
        setFormat(span.start, span.length, style);
    }
}

void SourceHighlighter::applyFormat(int offset, int length, const KSyntaxHighlighting::Format &style) {
    KSyntaxHighlighting::SyntaxHighlighter::applyFormat(offset, length, style);
    if (style.textStyle() == KSyntaxHighlighting::Theme::Keyword || style.textStyle() == KSyntaxHighlighting::Theme::ControlFlow) {
        auto colored = format(offset);
        colored.setForeground(colorFor("keyword"));
        setFormat(offset, length, colored);
    }
}
