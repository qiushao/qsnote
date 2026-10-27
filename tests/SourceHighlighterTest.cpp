#include "ui/SourceHighlighter.h"
#include <Definition>
#include <QApplication>
#include <QPlainTextEdit>
#include <QTest>
#include <QTextBlock>
#include <QTextLayout>
#include <Repository>
#include <Theme>
#include <cstdlib>

namespace {
void check(bool condition, const QString &message) {
    if (condition) return;
    qCritical().noquote() << message;
    std::exit(1);
}

QColor colorAt(const QPlainTextEdit &editor, const QString &token, int occurrence = 0) {
    const auto source = editor.toPlainText();
    int position = -1;
    for (int index = 0; index <= occurrence; ++index) position = static_cast<int>(source.indexOf(token, position + 1));
    check(position >= 0, "Missing token: " + token);
    const auto block = editor.document()->findBlock(position);
    const int offset = position - block.position();
    for (const auto &range: block.layout()->formats()) {
        if (offset >= range.start && offset < range.start + range.length) return range.format.foreground().color();
    }
    return editor.palette().color(QPalette::Text);
}

void expectColor(const QPlainTextEdit &editor, const QString &token, const QColor &expected, int occurrence = 0) {
    const auto actual = colorAt(editor, token, occurrence);
    check(actual == expected, token + ": expected " + expected.name() + ", got " + actual.name());
}

void checkLanguage(const QString &fileName, const QString &source, const QString &keyword, const QString &type, const QString &function, const QString &property, bool dark) {
    QPlainTextEdit editor;
    new SourceHighlighter(editor.document(), fileName, dark);
    editor.setPlainText(source);
    QTest::qWait(180);
    if (!keyword.isEmpty()) expectColor(editor, keyword, QColor(dark ? "#c586c0" : "#af00db"));
    if (!type.isEmpty()) expectColor(editor, type, QColor(dark ? "#4ec9b0" : "#267f99"));
    expectColor(editor, function, QColor(dark ? "#dcdcaa" : "#795e26"));
    expectColor(editor, property, QColor(dark ? "#9cdcfe" : "#0057ae"));
    check(editor.toPlainText() == source, fileName + ": highlighting changed source");
}
}// namespace

int main(int argc, char **argv) {
    const QApplication app(argc, argv);
    // Exercise the original lexical engine directly: a disconnected QStringView
    // previously made every keyword lookup fail, including case-insensitive lists.
    KSyntaxHighlighting::Repository repository;
    repository.addCustomSearchPath(":/demo/data");
    for (const auto &sample: {qMakePair(QString("sample.cpp"), QString("class Widget { int value; };")),
                              qMakePair(QString("sample.sql"), QString("sElEcT value FROM records;"))}) {
        QPlainTextEdit lexical;
        auto *highlighter = new KSyntaxHighlighting::SyntaxHighlighter(lexical.document());
        const auto theme = repository.defaultTheme(KSyntaxHighlighting::Repository::LightTheme);
        highlighter->setTheme(theme);
        highlighter->setDefinition(repository.definitionForFileName(sample.first));
        lexical.setPlainText(sample.second);
        highlighter->rehighlight();
        const auto formats = lexical.document()->firstBlock().layout()->formats();
        check(!formats.isEmpty() && formats.first().start == 0 && formats.first().length == sample.second.indexOf(QLatin1Char(' ')) && formats.first().format.fontWeight() == QFont::Bold,
              sample.first + ": lexical keyword matching failed");
    }
    for (const bool dark: {false, true}) {
        checkLanguage("sample.cpp", "class Widget { public: int value; void run() { this->value = 1; } };", "class", "Widget", "run", "value", dark);
        checkLanguage("sample.c", "struct Record { int value; }; void run(struct Record *r) { r->value = 1; }", "struct", "Record", "run", "value", dark);
        checkLanguage("Sample.java", "class Widget { private int value; void run() { this.value = 1; } }", "class", "Widget", "run", "value", dark);
        checkLanguage("sample.py", "class Widget:\n    def run(self):\n        self.value = 1\n", "class", "Widget", "run", "value", dark);
        checkLanguage("sample.js", "class Widget { value = 1; run() { return this.value; } }", "class", "Widget", "run", "value", dark);
        checkLanguage("sample.ts", "class Widget { value: number = 1; run(): number { return this.value; } }", "class", "Widget", "run", "value", dark);
        checkLanguage("sample.tsx", "class Widget { value = <div />; run() { return this.value; } }", "class", "Widget", "run", "value", dark);
        checkLanguage("sample.sh", "run() { local value=1; if true; then echo \"$value\"; fi; }", "if", {}, "run", "value", dark);
    }

    QPlainTextEdit editor;
    new SourceHighlighter(editor.document(), "sample.cpp", false);
    editor.setPlainText(QStringLiteral("/* 中文😀 */ class Widget { int value; void run(); };\nvoid Widget::run() { this->value = 1; }\n// class Fake { void nope(); };\nconst char *s = \"class Fake { void nope(); }\";"));
    QTest::qWait(180);
    expectColor(editor, "Widget", QColor("#267f99"));
    expectColor(editor, "run", QColor("#795e26"), 1);
    expectColor(editor, "value", QColor("#0057ae"), 1);
    check(colorAt(editor, "Fake") == colorAt(editor, "//"), "class in comment was colored as code");
    check(colorAt(editor, "Fake", 1) == colorAt(editor, "\"class"), "class in string was colored as code");

    // Insert before the first symbol, then turn code into a multiline comment and undo.
    auto cursor = editor.textCursor();
    cursor.setPosition(0);
    cursor.insertText("\n/*\n");
    QTest::qWait(180);
    // The original closing comment ends this inserted comment; the class remains code.
    expectColor(editor, "Widget", QColor("#267f99"));
    editor.undo();
    QTest::qWait(180);
    expectColor(editor, "Widget", QColor("#267f99"));
    editor.setPlainText("class Widget {\n  int value;\n  void run() {}\n};\n");
    QTest::qWait(180);
    cursor = editor.textCursor();
    cursor.setPosition(0);
    cursor.insertText("/*\n");
    cursor.movePosition(QTextCursor::End);
    cursor.insertText("*/");
    QTest::qWait(180);
    check(colorAt(editor, "Widget") == colorAt(editor, "/*"), "stale class highlighting after editing");
    check(colorAt(editor, "run") == colorAt(editor, "/*"), "stale function highlighting after editing");

    checkLanguage("sample.hpp", "class Widget { int value; void run(); };", "class", "Widget", "run", "value", false);
    checkLanguage("sample.mjs", "class Widget { value = 1; run() {} }", "class", "Widget", "run", "value", false);

    for (const auto &sample: {qMakePair(QString("calls.cpp"), QString("void run() { object.run(); ns::run(); }")),
                              qMakePair(QString("calls.js"), QString("const run = () => object.run(); run();")),
                              qMakePair(QString("calls.py"), QString("def run():\n    object.run()\n    run()\n"))}) {
        QPlainTextEdit calls;
        new SourceHighlighter(calls.document(), sample.first, false);
        calls.setPlainText(sample.second);
        QTest::qWait(180);
        for (int occurrence = 0; occurrence < 3; ++occurrence) expectColor(calls, "run", QColor("#795e26"), occurrence);
    }

    QPlainTextEdit plain;
    new SourceHighlighter(plain.document(), "sample.txt", false);
    plain.setPlainText("class Widget { value; run(); }");
    QTest::qWait(180);
    check(plain.document()->firstBlock().layout()->formats().isEmpty(), "plain text acquired syntax colors");
    qInfo() << "Source highlighting regression tests passed";
}
