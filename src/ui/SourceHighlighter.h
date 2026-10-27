#ifndef QSNOTE_SOURCEHIGHLIGHTER_H
#define QSNOTE_SOURCEHIGHLIGHTER_H

#include <QHash>
#include <QTimer>
#include <SyntaxHighlighter>
#include <memory>
#include <tree_sitter/api.h>

// Keep KSyntaxHighlighting's lexical rules and supplement identifier roles with the AST.
class SourceHighlighter : public KSyntaxHighlighting::SyntaxHighlighter {
public:
    SourceHighlighter(QTextDocument *document, const QString &fileName, bool dark);

protected:
    void highlightBlock(const QString &text) override;
    void applyFormat(int offset, int length, const KSyntaxHighlighting::Format &style) override;

private:
    struct Span {
        int start;
        int length;
        QColor color;
    };
    void updateSymbols();
    QColor colorFor(const QByteArray &capture) const;

    bool dark_;
    QTimer timer_;
    QHash<int, QVector<Span>> spans_;
    std::unique_ptr<TSParser, decltype(&ts_parser_delete)> parser_{nullptr, ts_parser_delete};
    std::unique_ptr<TSQuery, decltype(&ts_query_delete)> query_{nullptr, ts_query_delete};
};

#endif
