#include "core/NotebookStore.h"
#include "core/EbookExporter.h"
#include "ui/MainWindow.h"
#include "ui/MarkdownEditor.h"
#include "ui/MarkdownPreview.h"
#include "ui/InplacePreview.h"
#include <QWebEnginePage>
#include <QWebEngineView>
#include <QEventLoop>
#include <QPointer>
#include <QProcess>
#include <QSignalSpy>
#include <QScopeGuard>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextLayout>
#include <QDialog>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QSettings>
#include <QSpinBox>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QInputDialog>
#include <QLabel>
#include <QJsonArray>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <vtextedit/vtextedit.h>
#include <vtextedit/texteditorconfig.h>
#include <vtextedit/previewdata.h>
#include <vtextedit/previewmgr.h>
#include <QPushButton>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTabBar>
#include <QTemporaryDir>
#include <QTest>
#include <QTextBrowser>
#include <QTimer>
#include <QTreeWidget>
#include <QWheelEvent>

namespace {
QString fixturePath(const QString &name) {
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/notebooks/" + name;
}
}// namespace

// Qt Test discovers non-static slots through the meta-object system.
// NOLINTBEGIN(readability-convert-member-functions-to-static)
class UiTest : public QObject {
    Q_OBJECT
    static void openTreeNote(QTreeWidget *tree, QTreeWidgetItem *item) {
        const auto position = tree->visualItemRect(item).center();
        QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier, position);
        QTest::mouseDClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier, position);
    }
    static QVariant javascript(QWebEngineView &preview, const QString &script) {
        QEventLoop loop;
        QVariant result;
        preview.page()->runJavaScript(script, [guard = QPointer<QEventLoop>(&loop), &result](const QVariant &value) {
            if (!guard) return;
            result = value;
            guard->quit();
        });
        QTimer::singleShot(5000, &loop, &QEventLoop::quit);
        loop.exec();
        return result;
    }
    static QList<vte::PreviewImageData> previewImages(MarkdownEditor &editor, vte::PreviewData::Source source) {
        QList<vte::PreviewImageData> images;
        for (auto block = editor.document()->begin(); block.isValid(); block = block.next()) {
            for (const auto data: vte::BlockPreviewData::get(block)->getPreviewData()) {
                if (data->source() == source) images.append(*data->getImageData());
            }
        }
        return images;
    }
private slots:
    void init() {
        QSettings().clear();
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(QDir().mkpath(QFileInfo(NotebookStore::storagePath()).absolutePath()));
        QFile::remove(NotebookStore::storagePath());
        QDir(fixturePath({})).removeRecursively();
    }

    void markdownFormattingAndUndo() {
        MarkdownEditor editor;
        editor.resize(700, 500);
        editor.show();
        editor.setText("- 第一行\n- 第二行");
        editor.getTextEdit()->selectAll();
        editor.type(vte::TypeAction::TypeBold);
        QCOMPARE(editor.getText(), QString("- **第一行**\n- **第二行**"));
        editor.getTextEdit()->undo();
        QCOMPARE(editor.getText(), QString("- 第一行\n- 第二行"));
        editor.getTextEdit()->redo();
        editor.getTextEdit()->selectAll();
        editor.type(vte::TypeAction::TypeBold);
        QCOMPARE(editor.getText(), QString("- 第一行\n- 第二行"));
    }

    void fencedCodeSyntaxHighlighting_data() { // NOLINT(readability-identifier-naming): Qt Test data slot.
        QTest::addColumn<QString>("language");
        QTest::addColumn<QString>("code");
        QTest::addColumn<int>("commentPosition");
        const QString python = "def greet():\n    return \"hello\" # comment\n";
        QTest::newRow("python") << QString("python") << python << 19;
        QTest::newRow("py") << QString("py") << python << 19;
        QTest::newRow("c") << QString("c") << QString("int greet() {\n    return \"hello\"; // comment\n}\n") << 20;
    }

    void fencedCodeSyntaxHighlighting() {
        QFETCH(QString, language);
        QFETCH(QString, code);
        QFETCH(int, commentPosition);
        QSettings().setValue("editor/inplacePreview", false);
        MarkdownEditor editor;
        editor.resize(700, 500);
        editor.show();
        const QString source = "```" + language + "\n" + code + "```\n";
        editor.setText(source);
        const int undoSteps = editor.document()->availableUndoSteps();
        const auto formatAt = [&editor](int line, int position) {
            const auto block = editor.document()->findBlockByNumber(line);
            for (const auto &range: block.layout()->formats()) {
                if (range.start <= position && position < range.start + range.length) {
                    return range.format;
                }
            }
            return QTextCharFormat();
        };
        QTRY_COMPARE_WITH_TIMEOUT(formatAt(1, 0).foreground().color(), QColor("#0077aa"), 15000);
        QCOMPARE(formatAt(1, 4).foreground().color(), QColor("#dd4a68"));
        QCOMPARE(formatAt(2, 11).foreground().color(), QColor("#669900"));
        QCOMPARE(formatAt(2, commentPosition).foreground().color(), QColor("#708090"));
        QCOMPARE(editor.getText(), source);
        QCOMPARE(editor.document()->availableUndoSteps(), undoSteps);

        MarkdownPreview preview;
        preview.setMarkdown(source, "highlight");
        QTRY_COMPARE_WITH_TIMEOUT(preview.renderedRevision(), preview.revision(), 15000);
        QCOMPARE(javascript(preview, "getComputedStyle(document.querySelector('.token.keyword')).color").toString(), QString("rgb(0, 119, 170)"));
        QCOMPARE(javascript(preview, "getComputedStyle(document.querySelector('.token.string')).color").toString(), QString("rgb(102, 153, 0)"));
        QCOMPARE(javascript(preview, "getComputedStyle(document.querySelector('.token.comment')).color").toString(), QString("rgb(112, 128, 144)"));
    }

    void fencedCodeHighlightingMultilineAndStaleResults() {
        QSettings().setValue("editor/inplacePreview", false);
        MarkdownEditor editor;
        editor.resize(700, 500);
        editor.show();
        editor.setText("```c\nint old = 42;\n```\n");
        const QString source = "~~~python\nmessage = \"\"\"first <&>\nsecond <&>\n\"\"\"\ndef greet():\n    return message\n~~~\n";
        editor.setText(source);
        auto cursor = editor.getTextEdit()->textCursor();
        cursor.setPosition(static_cast<int>(source.indexOf("second")));
        cursor.setPosition(cursor.position() + 6, QTextCursor::KeepAnchor);
        editor.getTextEdit()->setTextCursor(cursor);
        editor.reloadSettings();
        const int undoSteps = editor.document()->availableUndoSteps();
        const auto colorAt = [&editor](int line, int position) {
            for (const auto &range: editor.document()->findBlockByNumber(line).layout()->formats()) {
                if (range.start <= position && position < range.start + range.length) return range.format.foreground().color();
            }
            return QColor();
        };
        QTRY_COMPARE_WITH_TIMEOUT(colorAt(4, 0), QColor("#0077aa"), 15000);
        QCOMPARE(colorAt(1, 10), QColor("#669900"));
        QCOMPARE(colorAt(2, 0), QColor("#669900"));
        QCOMPARE(colorAt(3, 0), QColor("#669900"));
        QCOMPARE(editor.getText(), source);
        QCOMPARE(editor.document()->availableUndoSteps(), undoSteps);
        QCOMPARE(editor.getTextEdit()->textCursor().anchor(), cursor.anchor());
        QCOMPARE(editor.getTextEdit()->textCursor().position(), cursor.position());
        editor.insertText("changed");
        editor.getTextEdit()->undo();
        QCOMPARE(editor.getText(), source);
        QTRY_COMPARE(colorAt(2, 0), QColor("#669900"));
    }

    void persistentEditorSettings() {
        MarkdownEditor editor;
        editor.resize(700, 600);
        editor.show();
        editor.setText("Original");
        editor.getTextEdit()->moveCursor(QTextCursor::End);
        editor.insertText(" edit");
        auto selection = editor.getTextEdit()->textCursor();
        selection.setPosition(0);
        selection.setPosition(4, QTextCursor::KeepAnchor);
        editor.getTextEdit()->setTextCursor(selection);
        QSignalSpy changed(&editor, &MarkdownEditor::settingsChanged);
        QTimer::singleShot(20, &editor, [] {
            auto *dialog = QApplication::activeModalWidget();
            QVERIFY(dialog);
            QVERIFY(!dialog->findChild<QCheckBox *>("autoFoldPreviews")->isChecked());
            dialog->findChild<QSpinBox *>("fontSize")->setValue(24);
            qobject_cast<QDialog *>(dialog)->reject();
        });
        editor.showSettings();
        QCOMPARE(changed.count(), 0);
        QVERIFY(!QSettings().contains("editor/fontSize"));
        const int defaultSize = editor.baseEditorFontPointSize();
        QTimer::singleShot(20, &editor, [] {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            dialog->findChild<QComboBox *>("lineNumbers")->setCurrentIndex(2);
            dialog->findChild<QComboBox *>("inputMode")->setCurrentIndex(2);
            dialog->findChild<QComboBox *>("wrapMode")->setCurrentIndex(0);
            dialog->findChild<QComboBox *>("centerCursor")->setCurrentIndex(2);
            dialog->findChild<QSpinBox *>("fontSize")->setValue(16);
            dialog->findChild<QSpinBox *>("zoom")->setValue(2);
            dialog->findChild<QSpinBox *>("tabWidth")->setValue(2);
            dialog->findChild<QDoubleSpinBox *>("lineSpacing")->setValue(1.5);
            dialog->findChild<QSpinBox *>("contentWidth")->setValue(600);
            dialog->findChild<QCheckBox *>("expandTab")->setChecked(false);
            dialog->findChild<QCheckBox *>("previewTables")->setChecked(false);
            dialog->findChild<QCheckBox *>("formatTables")->setChecked(false);
            dialog->findChild<QCheckBox *>("numberLists")->setChecked(false);
            dialog->grab().save("editor-settings.png");
            dialog->accept();
        });
        editor.showSettings();
        QCOMPARE(changed.count(), 1);
        QCOMPARE(editor.getText(), QString("Original edit"));
        QCOMPARE(editor.getTextEdit()->textCursor().anchor(), selection.anchor());
        QCOMPARE(editor.getTextEdit()->textCursor().position(), selection.position());
        QCOMPARE(editor.getConfig().m_lineNumberType, vte::VTextEditor::LineNumberType::Relative);
        QCOMPARE(editor.getConfig().m_wrapMode, vte::NoWrap);
        QCOMPARE(editor.getConfig().m_centerCursor, vte::CenterOnBottom);
        QCOMPARE(editor.getConfig().m_tabStopWidth, 2);
        QCOMPARE(editor.getConfig().m_lineSpacing, 1.5);
        QCOMPARE(editor.getConfig().m_maxContentWidth, 600);
        QCOMPARE(editor.baseEditorFontPointSize(), 16);
        QCOMPARE(editor.editorFontPointSize(), 18);
        QCOMPARE(editor.getTextEdit()->font().pointSize(), 18);
        editor.reloadSettings();
        QCOMPARE(editor.editorFontPointSize(), 18);
        editor.getTextEdit()->undo();
        QCOMPARE(editor.getText(), QString("Original"));
        editor.getTextEdit()->redo();
        QCOMPARE(editor.getText(), QString("Original edit"));
        QSettings disk(QSettings().fileName(), QSettings::IniFormat);
        QCOMPARE(disk.value("editor/fontSize").toInt(), 16);
        MarkdownEditor reopened;
        QCOMPARE(reopened.getConfig().m_lineNumberType, vte::VTextEditor::LineNumberType::Relative);
        QCOMPARE(reopened.getConfig().m_inputMode, vte::VscodeMode);
        QCOMPARE(reopened.editorFontPointSize(), 18);
        QTest::keyClick(reopened.getTextEdit(), Qt::Key_Tab);
        QCOMPARE(reopened.getText(), QString("\t"));
        // Status-bar spell controls also persist, not just the settings dialog.
        reopened.setAutoDetectLanguageEnabled(true);
        QVERIFY(QSettings().value("editor/autoDetectLanguage").toBool());
        QSettings().clear();
        MarkdownEditor defaults;
        QCOMPARE(defaults.baseEditorFontPointSize(), defaultSize);
    }

    void controlWheelPersistsEditorZoom() {
        QSettings().setValue("editor/fontSize", 12);
        MarkdownEditor editor;
        editor.resize(700, 500);
        editor.show();
        editor.setText(QString("Line\n").repeated(200));
        editor.insertText("Edited ");
        const auto source = editor.getText();
        const int undoSteps = editor.document()->availableUndoSteps();
        const auto cursor = editor.getTextEdit()->textCursor();
        auto *viewport = editor.getTextEdit()->viewport();
        const auto wheel = [viewport](int delta, Qt::KeyboardModifiers modifiers) {
            const auto position = viewport->rect().center();
            QWheelEvent event(position, viewport->mapToGlobal(position), {}, QPoint(0, delta),
                              Qt::NoButton, modifiers, Qt::NoScrollPhase, false);
            QApplication::sendEvent(viewport, &event);
        };
        QSignalSpy changed(&editor, &MarkdownEditor::settingsChanged);
        QTRY_VERIFY(editor.getTextEdit()->verticalScrollBar()->maximum() > 0);
        editor.getTextEdit()->verticalScrollBar()->setValue(0);
        wheel(-120, Qt::NoModifier);
        QVERIFY(editor.getTextEdit()->verticalScrollBar()->value() > 0);
        QCOMPARE(editor.editorFontPointSize(), 12);
        QVERIFY(!QSettings().contains("editor/zoom"));

        wheel(120, Qt::ControlModifier);
        QCOMPARE(editor.editorFontPointSize(), 13);
        QCOMPARE(editor.getTextEdit()->font().pointSize(), 13);
        wheel(240, Qt::ControlModifier);
        QCOMPARE(editor.editorFontPointSize(), 15);
        wheel(-120, Qt::ControlModifier);
        QCOMPARE(editor.editorFontPointSize(), 14);
        QCOMPARE(changed.count(), 3);
        QCOMPARE(editor.getText(), source);
        QCOMPARE(editor.document()->availableUndoSteps(), undoSteps);
        QCOMPARE(editor.getTextEdit()->textCursor().position(), cursor.position());
        QCOMPARE(editor.getTextEdit()->textCursor().anchor(), cursor.anchor());
        QSettings disk(QSettings().fileName(), QSettings::IniFormat);
        QCOMPARE(disk.value("editor/zoom").toInt(), 2);
        editor.reloadSettings();
        QCOMPARE(editor.editorFontPointSize(), 14);
        MarkdownEditor reopened;
        QCOMPARE(reopened.editorFontPointSize(), 14);

        wheel(-2400, Qt::ControlModifier);
        QCOMPARE(editor.editorFontPointSize(), 2);
        wheel(-120, Qt::ControlModifier);
        QCOMPARE(editor.editorFontPointSize(), 2);
        QCOMPARE(changed.count(), 4);
        editor.reloadSettings();
        QCOMPARE(editor.editorFontPointSize(), 2);
        wheel(120, Qt::ControlModifier);
        QCOMPARE(editor.editorFontPointSize(), 3);
    }

    void markdownFindReplaceDialog() {
        MarkdownEditor editor;
        editor.setText("alpha ALPHA alpha");
        QTimer::singleShot(30, &editor, [] {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            dialog->findChild<QLineEdit *>("findText")->setText("alpha");
            dialog->findChild<QLineEdit *>("replaceText")->setText("beta");
            QTest::mouseClick(dialog->findChild<QPushButton *>("findAction3"), Qt::LeftButton);
            dialog->accept();
        });
        editor.showFindReplace();
        QCOMPARE(editor.getText(), QString("beta beta beta"));
        editor.getTextEdit()->undo();
        QCOMPARE(editor.getText(), QString("alpha ALPHA alpha"));
    }

    void markdownInteractiveTable() {
        MarkdownEditor editor;
        editor.resize(700, 600);
        editor.show();
        const QString source = "Intro\n\n| A | B |\n| --- | --- |\n| one | two |\n\nEnd\n";
        editor.setText(source);
        QTRY_VERIFY(!editor.getVisiblePreviewWidgetLocations().isEmpty());
        const auto previews = editor.getVisiblePreviewWidgetLocations();
        QVERIFY(editor.focusPreviewWidget(previews.first().m_identity));
        auto *cell = qobject_cast<QTextEdit *>(QApplication::focusWidget());
        QVERIFY(cell);
        QVERIFY(cell != editor.getTextEdit());
        QTest::keyClicks(cell, "edited");
        QTRY_VERIFY(editor.getText().contains("edited"));
        QVERIFY(editor.getText().contains("End"));
    }

    void inplacePreviewRendersMathAndDiagrams() {
        QSettings().setValue("editor/autoFoldPreviews", false);
        MarkdownEditor editor;
        editor.resize(1000, 1000);
        editor.show();
        const QString source = R"md(# In-place preview

$E=mc^2$ and $$\frac{a}{b}$$

$$
\sum_{i=1}^{n} i^2
$$

| Formula | Value |
| --- | --- |
| $x^2$ | 2 |

```MeRmAiD title
graph TD
A[开始] --> B[结束]
```

> ```flowchart
> st=>start: Start
> e=>end: End
> st->e
> ```

```graphviz
digraph G { a -> b }
```

```wave
{"signal": [{"name": "clk", "wave": "p...."}]}
```

```mathjax
\frac{1}{2}
```

```cpp
int ordinary_code = 1;
```
)md";
        editor.setText(source);
        QTRY_COMPARE_WITH_TIMEOUT(previewImages(editor, vte::PreviewData::MathBlock).size(), 4, 30000);
        QTRY_COMPARE_WITH_TIMEOUT(previewImages(editor, vte::PreviewData::CodeBlock).size(), 5, 30000);
        const auto math = previewImages(editor, vte::PreviewData::MathBlock);
        QVERIFY(math[0].m_inline);
        QVERIFY(math[0].m_imageSize.width() > 2 * math[0].m_imageSize.height());
        QVERIFY(math[1].m_inline); // Single-line $$ uses display typesetting, inline placement.
        QVERIFY(!math[2].m_inline);
        QVERIFY(math[3].m_inline); // Also delivered to the interactive table sheet.
        for (auto type: {vte::PreviewData::MathBlock, vte::PreviewData::CodeBlock}) {
            for (const auto &image: previewImages(editor, type)) {
                const auto pixmap = editor.findImageFromDocumentResourceMgr(image.m_imageName);
                QVERIFY(pixmap && !pixmap->isNull());
                QVERIFY(image.m_imageSize.width() > 5 && image.m_imageSize.height() > 5);
                // A successful transport must contain visible pixels, including KaTeX's embedded fonts.
                const auto pixels = pixmap->toImage();
                int visible = 0;
                for (int y = 0; y < pixels.height(); ++y) {
                    for (int x = 0; x < pixels.width(); ++x) {
                        const auto color = pixels.pixelColor(x, y);
                        if (color.alpha() > 0 && color.lightness() < 240) ++visible;
                    }
                }
                QVERIFY(visible > 20);
            }
        }
        QCOMPARE(editor.getText(), source);
        QVERIFY(!editor.document()->isModified());
        editor.grab().save("inplace-preview.png");

        const auto oldSize = math[0].m_imageSize;
        editor.zoom(8);
        QTRY_COMPARE_WITH_TIMEOUT(previewImages(editor, vte::PreviewData::MathBlock).size(), 4, 30000);
        QTRY_VERIFY(previewImages(editor, vte::PreviewData::MathBlock).size() == 4 && previewImages(editor, vte::PreviewData::MathBlock)[0].m_imageSize.height() > oldSize.height());
        QCOMPARE(editor.getText(), source);
    }

    void inplacePreviewSettingsAndStaleResults() {
        MarkdownEditor editor;
        editor.resize(700, 500);
        editor.show();
        editor.setText("Plain text");
        QTest::qWait(300);
        QVERIFY(!editor.findChild<QWebEnginePage *>());
        auto *renderer = editor.findChild<InplacePreview *>();
        QVERIFY(renderer);
        QSignalSpy requests(renderer, &InplacePreview::renderRequested);
        editor.setText("```mermaid\ngraph TD\nA-->B\n```\n\n$x$");
        QTRY_VERIFY_WITH_TIMEOUT(!requests.isEmpty(), 30000);
        editor.setText("Latest document");
        QTest::qWait(1000);
        QCOMPARE(previewImages(editor, vte::PreviewData::MathBlock).size(), 0);
        QCOMPARE(previewImages(editor, vte::PreviewData::CodeBlock).size(), 0);
        QCOMPARE(editor.getText(), QString("Latest document"));

        const QString source = "$x^2$\n\n```dot\ndigraph G { a -> b }\n```";
        editor.setText(source);
        QTRY_COMPARE(previewImages(editor, vte::PreviewData::MathBlock).size(), 1);
        QTRY_COMPARE(previewImages(editor, vte::PreviewData::CodeBlock).size(), 1);
        bool clearedUnchangedFormula = false;
        const auto publication = connect(editor.getPreviewMgr(), &vte::PreviewMgr::previewDataUpdated, &editor,
                [&clearedUnchangedFormula](vte::PreviewData::Source type, const QVector<QTextBlock> &blocks) {
                    if (type == vte::PreviewData::MathBlock && blocks.isEmpty()) clearedUnchangedFormula = true;
                });
        editor.getTextEdit()->moveCursor(QTextCursor::End);
        editor.insertText("\nUnrelated paragraph");
        QTest::qWait(500);
        QVERIFY(!clearedUnchangedFormula);
        disconnect(publication);
        editor.getTextEdit()->undo();
        QCOMPARE(editor.getText(), source);
        QSettings().setValue("editor/previewMath", false);
        editor.reloadSettings();
        QTRY_COMPARE(previewImages(editor, vte::PreviewData::MathBlock).size(), 0);
        QTRY_COMPARE(previewImages(editor, vte::PreviewData::CodeBlock).size(), 1);
        QSettings().setValue("editor/previewGraphs", false);
        editor.reloadSettings();
        QTRY_COMPARE(previewImages(editor, vte::PreviewData::CodeBlock).size(), 0);
        QSettings().setValue("editor/previewGraphs", true);
        QSettings().setValue("editor/inplacePreview", false);
        editor.reloadSettings();
        QTRY_COMPARE(previewImages(editor, vte::PreviewData::CodeBlock).size(), 0);
        QSettings().setValue("editor/previewMath", true);
        QSettings().setValue("editor/inplacePreview", true);
        editor.reloadSettings();
        QTRY_COMPARE(previewImages(editor, vte::PreviewData::MathBlock).size(), 1);
        QTRY_COMPARE(previewImages(editor, vte::PreviewData::CodeBlock).size(), 1);
        QCOMPARE(editor.getText(), source);
        editor.getTextEdit()->moveCursor(QTextCursor::End);
        editor.insertText("\nedit");
        editor.getTextEdit()->undo();
        QCOMPARE(editor.getText(), source);
        QTRY_COMPARE(previewImages(editor, vte::PreviewData::MathBlock).size(), 1);
    }

    void inplacePreviewFailuresAndSourceFolding() {
        QSettings().setValue("editor/autoFoldPreviews", true);
        MarkdownEditor editor;
        editor.resize(700, 600);
        editor.show();
        const QString source = "Intro\n\n```dot\ndigraph G { a -> b }\n```\n\n"
                               "```mermaid\ninvalid diagram\n```\n\n$\\unknowncommand{x}$\n\n$y^2$";
        editor.setText(source);
        QTRY_COMPARE_WITH_TIMEOUT(previewImages(editor, vte::PreviewData::CodeBlock).size(), 1, 30000);
        QTRY_COMPARE_WITH_TIMEOUT(previewImages(editor, vte::PreviewData::MathBlock).size(), 1, 30000);
        QTRY_VERIFY(!editor.document()->findBlockByNumber(3).isVisible());
        QVERIFY(editor.document()->findBlockByNumber(7).isVisible());
        QCOMPARE(editor.getText(), source);
        editor.setText("No previews");
        QTRY_COMPARE(previewImages(editor, vte::PreviewData::CodeBlock).size(), 0);
        QTRY_COMPARE(previewImages(editor, vte::PreviewData::MathBlock).size(), 0);
    }

    void inplacePreviewRendersPlantUml() {
        if (QStandardPaths::findExecutable("plantuml").isEmpty()) QSKIP("Install PlantUML to run diagram integration test");
        MarkdownEditor editor;
        editor.resize(700, 500);
        editor.show();
        const QString source = "Intro\n\n```puml\nAlice -> Bob: hello\n```";
        editor.setText(source);
        QTRY_COMPARE_WITH_TIMEOUT(previewImages(editor, vte::PreviewData::CodeBlock).size(), 1, 60000);
        QVERIFY(previewImages(editor, vte::PreviewData::CodeBlock)[0].m_imageSize.width() > 20);
        QCOMPARE(editor.getText(), source);
    }

    void inplacePreviewGraphvizEngines() {
        QSettings().setValue("editor/autoFoldPreviews", false);
        MarkdownEditor editor;
        editor.resize(700, 500);
        editor.show();
        QString source;
        QStringList engines = {"dot", "neato", "twopi", "circo", "fdp", "osage", "patchwork"};
        if (!QStandardPaths::findExecutable("dot").isEmpty()) engines.append("sfdp");
        for (const auto &engine: engines) source += "```" + engine + "\ndigraph G { a -> b }\n```\n\n";
        editor.setText(source);
        QTRY_COMPARE_WITH_TIMEOUT(previewImages(editor, vte::PreviewData::CodeBlock).size(), engines.size(), 30000);
        QCOMPARE(editor.getText(), source);
    }

    void inplacePreviewMissingPlantUml() {
        const auto path = qgetenv("PATH");
        const auto restore = qScopeGuard([path] { qputenv("PATH", path); });
        qputenv("PATH", "");
        MarkdownEditor editor;
        editor.resize(700, 500);
        editor.show();
        const QString source = "```plantuml\nAlice -> Bob\n```\n\n```sfdp\ndigraph G { a -> b }\n```\n\n```dot\ndigraph G { a -> b }\n```";
        editor.setText(source);
        QTRY_COMPARE_WITH_TIMEOUT(previewImages(editor, vte::PreviewData::CodeBlock).size(), 1, 30000);
        QVERIFY(editor.document()->findBlockByNumber(1).isVisible());
        QCOMPARE(editor.getText(), source);
    }

    void webPreviewRendersExtensionsAndDiagrams() {
        MarkdownPreview preview;
        preview.resize(900, 700);
        preview.show();
        preview.setMarkdown(QString::fromUtf8(R"md(# 预览验证

$E=mc^2$ and $$x^2$$

==重点== H~2~O x^2^ :smile:

- [ ] task

| a | b |
| --- | --- |
| 1 | 2 |

Footnote[^1]

[^1]: Footnote text

```cpp
int main() { return 0; }
```

```mermaid
graph TD
A[开始] --> B[结束]
```

```dot
digraph G { a -> b }
```

```flow
st=>start: Start
e=>end: End
st->e
```

```wavedrom
{"signal": [{"name": "clk", "wave": "p...."}]}
```
)md"), "render-test");
        QTRY_COMPARE_WITH_TIMEOUT(preview.renderedRevision(), preview.revision(), 30000);
        QVERIFY(javascript(preview, "document.querySelectorAll('.katex').length >= 2").toBool());
        QVERIFY(javascript(preview, "Math.abs(document.querySelectorAll('eq')[0].getBoundingClientRect().top - document.querySelectorAll('eq')[1].getBoundingClientRect().top) < 20").toBool());
        QCOMPARE(javascript(preview, "document.querySelectorAll('.diagram svg').length").toInt(), 4);
        QCOMPARE(javascript(preview, "document.querySelectorAll('.render-error').length").toInt(), 0);
        QVERIFY(javascript(preview, "!!document.querySelector('mark') && !!document.querySelector('sub') && !!document.querySelector('sup') && !!document.querySelector('.footnotes')").toBool());
        QVERIFY(javascript(preview, "!!document.querySelector('code .token.keyword')").toBool());
        QSignalSpy tasks(&preview, &MarkdownPreview::taskToggled);
        javascript(preview, "document.querySelector('input[type=checkbox]').click()");
        QTRY_COMPARE(tasks.count(), 1);
        QCOMPARE(tasks.first().at(1).toBool(), true);
        // WebEngine's DOM completion precedes delivery of its compositor frame.
        QTest::qWait(300);
        preview.grab().save("web-preview.png");
    }

    void readingFindAndHeadingFolding() {
        MarkdownPreview preview;
        preview.resize(900, 700);
        preview.show();
        preview.setHeadingFoldingEnabled(true);
        const QString source = "[Go to child](#child)\n\n# Parent\n\nneedle\n\n## Child\n\nNeedle\n\n# Other\n\nneedle\n";
        preview.setMarkdown(source, "fold-a");
        QTRY_COMPARE_WITH_TIMEOUT(preview.renderedRevision(), preview.revision(), 30000);
        QCOMPARE(javascript(preview, "document.querySelectorAll('.vx-heading-fold-toggle').length").toInt(), 3);
        javascript(preview, "document.querySelector('#parent .vx-heading-fold-toggle').click()");
        QVERIFY(javascript(preview, "document.querySelector('#parent').nextElementSibling.hidden").toBool());
        preview.setMarkdown(source + "\n```mermaid\ngraph TD\nA-->B\n```", "fold-a");
        preview.setMarkdown(source + "\nUpdated", "fold-a");
        QTRY_COMPARE(preview.renderedRevision(), preview.revision());
        QVERIFY(javascript(preview, "document.querySelector('#parent').nextElementSibling.hidden").toBool());
        javascript(preview, "document.querySelector('a[href=\"#child\"]').click()");
        QVERIFY(!javascript(preview, "document.querySelector('#parent').nextElementSibling.hidden").toBool());
        javascript(preview, "window.qsnotePreview.expandAll(false)");
        preview.scrollToLine(8);
        QTRY_VERIFY(!javascript(preview, "document.querySelector('#parent').nextElementSibling.hidden").toBool());
        QVERIFY(!javascript(preview, "document.querySelector('#child').nextElementSibling.hidden").toBool());
        javascript(preview, "window.qsnotePreview.expandAll(false)");
        preview.showFind();
        auto *dialog = preview.findChild<QDialog *>("previewFind");
        QVERIFY(dialog && dialog->isVisible());
        auto *query = dialog->findChild<QLineEdit *>("previewFindText");
        auto *result = dialog->findChild<QLabel *>("previewFindResult");
        query->setText("needle");
        QTRY_VERIFY(result->text().endsWith("/ 3 个匹配"));
        QCOMPARE(javascript(preview, "document.querySelectorAll('.vx-heading-fold-content[hidden]').length").toInt(), 0);
        dialog->findChild<QCheckBox *>()->setChecked(true);
        QTRY_VERIFY2(result->text().endsWith("/ 2 个匹配"), qPrintable(result->text()));
        const auto first = result->text();
        QTest::mouseClick(dialog->findChild<QPushButton *>("previewFindNext"), Qt::LeftButton);
        QTRY_VERIFY(result->text() != first && result->text().endsWith("/ 2 个匹配"));
        QTest::mouseClick(dialog->findChild<QPushButton *>("previewFindPrevious"), Qt::LeftButton);
        QTRY_COMPARE(result->text(), first);
        query->setText("absent");
        QTRY_COMPARE(result->text(), QString("0 / 0 个匹配"));
        query->setText("needle");
        preview.setMarkdown("# New\n\nneedle", "fold-b");
        QTRY_COMPARE(preview.renderedRevision(), preview.revision());
        QTRY_COMPARE(result->text(), QString("1 / 1 个匹配"));
        dialog->reject();
        QVERIFY(!dialog->isVisible());
        preview.setHeadingFoldingEnabled(false);
        QTRY_COMPARE(javascript(preview, "document.querySelectorAll('.vx-heading-fold-toggle').length").toInt(), 0);
        QCOMPARE(javascript(preview, "document.querySelectorAll('.vx-heading-fold-content[hidden]').length").toInt(), 0);
        QVERIFY(!QSettings().value("reader/headingFolding").toBool());
    }

    void readerSettingsAndHeadingNavigation() {
        MarkdownPreview preview;
        preview.resize(900, 500);
        preview.show();
        QString source = "# Parent\n\n";
        for (int i = 0; i < 30; ++i) source += "Paragraph before child.\n\n";
        source += "## Child\n\nNeedle\n\n# Other\n\n";
        for (int i = 0; i < 30; ++i) source += "Paragraph after child.\n\n";
        preview.setHeadingFoldingEnabled(true);
        preview.setMarkdown(source, "reader-a");
        QTRY_COMPARE_WITH_TIMEOUT(preview.renderedRevision(), preview.revision(), 30000);
        const auto original = javascript(preview, "getComputedStyle(document.documentElement).fontSize").toString();
        QTimer::singleShot(0, &preview, [&preview] {
            auto *dialog = preview.findChild<QDialog *>("readerSettings");
            QVERIFY(dialog);
            dialog->findChild<QSpinBox *>("readerFontSize")->setValue(24);
            dialog->reject();
        });
        preview.showSettings();
        QCOMPARE(javascript(preview, "getComputedStyle(document.documentElement).fontSize").toString(), original);
        QVERIFY(!QSettings().contains("reader/fontSize"));
        QString family;
        QTimer::singleShot(0, &preview, [&preview, &family] {
            auto *dialog = preview.findChild<QDialog *>("readerSettings");
            QVERIFY(dialog);
            dialog->findChild<QComboBox *>("readerTheme")->setCurrentIndex(1);
            auto *font = dialog->findChild<QComboBox *>("readerFont");
            QVERIFY(font->count() > 1);
            font->setCurrentIndex(1);
            family = font->currentData().toString();
            dialog->findChild<QSpinBox *>("readerFontSize")->setValue(24);
            dialog->findChild<QDoubleSpinBox *>("readerLineHeight")->setValue(2.0);
            dialog->findChild<QSpinBox *>("readerContentWidth")->setValue(600);
            dialog->accept();
        });
        preview.showSettings();
        QTRY_COMPARE(javascript(preview, "getComputedStyle(document.documentElement).fontSize").toString(), QString("24px"));
        QCOMPARE(javascript(preview, "getComputedStyle(document.documentElement).lineHeight").toString(), QString("48px"));
        QCOMPARE(javascript(preview, "getComputedStyle(document.documentElement).backgroundColor").toString(), QString("rgb(22, 27, 34)"));
        QCOMPARE(javascript(preview, "getComputedStyle(document.querySelector('main')).maxWidth").toString(), QString("600px"));
        QVERIFY(javascript(preview, "getComputedStyle(document.documentElement).fontFamily").toString().contains(family));
        QCOMPARE(QSettings().value("reader/fontSize").toInt(), 24);
        QVERIFY(preview.matchesDocument(source, "reader-a"));
        MarkdownPreview restored;
        restored.setMarkdown("# Restored", "reader-b");
        QTRY_COMPARE_WITH_TIMEOUT(restored.renderedRevision(), restored.revision(), 30000);
        QCOMPARE(javascript(restored, "getComputedStyle(document.documentElement).fontSize").toString(), QString("24px"));
        QCOMPARE(javascript(restored, "document.documentElement.dataset.theme").toString(), QString("dark"));

        javascript(preview, "window.qsnotePreview.expandAll(false)");
        preview.activateWindow();
        preview.setFocus();
        QTRY_VERIFY(preview.hasFocus());
        QTest::keyClick(preview.focusProxy(), Qt::Key_G, Qt::ControlModifier);
        QTRY_VERIFY(preview.findChild<QDialog *>("readerHeadings"));
        QPointer<QDialog> dialog = preview.findChild<QDialog *>("readerHeadings");
        auto *list = dialog->findChild<QListWidget *>("readerHeadingList");
        auto *query = dialog->findChild<QLineEdit *>("readerHeadingFilter");
        QTRY_COMPARE(list->count(), 3);
        query->setText("missing");
        QVERIFY(!list->currentItem());
        query->setText("cHiLd");
        QCOMPARE(list->currentRow(), 1);
        QTest::keyClick(query, Qt::Key_Return);
        QTRY_VERIFY(!dialog);
        QTRY_VERIFY(!javascript(preview, "document.querySelector('#parent').nextElementSibling.hidden").toBool());
        QTRY_VERIFY(javascript(preview, "(() => { const rect = document.querySelector('#child').getBoundingClientRect(); return scrollY > 100 && rect.top >= 0 && rect.bottom <= innerHeight; })()").toBool());
        preview.showHeadingNavigation();
        QPointer<QDialog> stale = preview.findChild<QDialog *>("readerHeadings");
        QVERIFY(stale);
        preview.setMarkdown("No headings", "reader-c");
        QTRY_VERIFY(!stale);
        QTRY_COMPARE(preview.renderedRevision(), preview.revision());
        QCOMPARE(javascript(preview, "window.scrollY").toInt(), 0);
        preview.showHeadingNavigation();
        dialog = preview.findChild<QDialog *>("readerHeadings");
        QVERIFY(dialog);
        QTRY_COMPARE(dialog->findChild<QLabel *>()->text(), QString("当前笔记没有标题"));
        dialog->close();
    }

    void webPreviewRendersPlantUml() {
        if (QStandardPaths::findExecutable("plantuml").isEmpty()) QSKIP("Install PlantUML to run diagram integration test");
        MarkdownPreview preview;
        preview.resize(900, 700);
        preview.show();
        preview.setMarkdown("# PlantUML\n\n```PlAnTuMl title\n@startuml\nAlice -> Bob: 你好\n@enduml\n```"
                            "\n\n> ```plantuml\n> Alice -> Bob: body-only\n> ```"
                            "\n\n```text\nordinary-code-preserved\n```", "plantuml");
        QTRY_COMPARE_WITH_TIMEOUT(preview.renderedRevision(), preview.revision(), 60000);
        QCOMPARE(javascript(preview, "document.querySelectorAll('.diagram img').length").toInt(), 2);
        QVERIFY(javascript(preview, "[...document.querySelectorAll('.diagram img')].every(image => image.complete && image.naturalWidth > 20 && image.naturalHeight > 20)").toBool());
        QCOMPARE(javascript(preview, "document.querySelectorAll('.render-error').length").toInt(), 0);
        QVERIFY(!javascript(preview, "!!document.querySelector('code.language-plantuml,code.language-PlAnTuMl')").toBool());
        QVERIFY(javascript(preview, "document.querySelector('code.language-text').textContent.includes('ordinary-code-preserved')").toBool());
        preview.grab().save("plantuml-preview.png");
        preview.setMarkdown("```plantuml\nAlice -> Bob: stale\n```", "old-plantuml");
        QTRY_VERIFY(javascript(preview, "!!document.querySelector('code.language-plantuml')").toBool() || preview.renderedRevision() == preview.revision());
        preview.setMarkdown("# Latest document", "new-plantuml");
        QTRY_COMPARE_WITH_TIMEOUT(preview.renderedRevision(), preview.revision(), 60000);
        QCOMPARE(javascript(preview, "document.querySelector('h1').textContent").toString(), QString("Latest document"));
        QCOMPARE(javascript(preview, "document.querySelectorAll('.diagram,.render-error').length").toInt(), 0);
    }

    void webPreviewReportsMissingPlantUml() {
        const auto path = qgetenv("PATH");
        const auto restorePath = qScopeGuard([&] { qputenv("PATH", path); });
        qputenv("PATH", QByteArray());
        MarkdownPreview preview;
        preview.show();
        preview.setMarkdown("```plantuml\nAlice -> Bob\n```", "missing-plantuml");
        QTRY_COMPARE_WITH_TIMEOUT(preview.renderedRevision(), preview.revision(), 30000);
        QVERIFY(javascript(preview, "document.querySelector('.render-error').textContent.includes('未找到 PlantUML')").toBool());
        QVERIFY(javascript(preview, "!!document.querySelector('code.language-plantuml')").toBool());
    }

    void webPreviewLatestDocumentAndScroll() {
        MarkdownPreview preview;
        preview.resize(700, 400);
        preview.show();
        preview.setMarkdown("# Old\n\n```mermaid\ngraph TD\na-->b\n```", "old");
        QString markdown;
        for (int i = 0; i < 80; ++i) markdown += QStringLiteral("## Heading %1\n\nParagraph %1\n\n").arg(i);
        preview.setMarkdown(markdown, "new");
        QTRY_COMPARE_WITH_TIMEOUT(preview.renderedRevision(), preview.revision(), 30000);
        QCOMPARE(javascript(preview, "document.querySelectorAll('h2').length").toInt(), 80);
        QVERIFY(!javascript(preview, "document.body.textContent.includes('Old')").toBool());
        preview.scrollToLine(160);
        QTRY_VERIFY(javascript(preview, "window.scrollY > 1000").toBool());
        QSignalSpy lines(&preview, &MarkdownPreview::sourceLineChanged);
        QTest::qWait(100);
        javascript(preview, "window.scrollTo(0, 700)");
        QTRY_VERIFY(!lines.isEmpty());
        QVERIFY(lines.last().first().toInt() > 0);
        preview.setMarkdown("# Third", "third");
        QTRY_COMPARE_WITH_TIMEOUT(preview.renderedRevision(), preview.revision(), 30000);
        QCOMPARE(javascript(preview, "window.scrollY").toInt(), 0);
        QCOMPARE(javascript(preview, "document.querySelectorAll('h2').length").toInt(), 0);
        preview.setMarkdown("<script>window.noteExecuted=true</script>\n\n<a href=\"javascript:window.noteExecuted=true\">bad</a>", "html");
        QTRY_COMPARE_WITH_TIMEOUT(preview.renderedRevision(), preview.revision(), 30000);
        QVERIFY(!javascript(preview, "window.noteExecuted === true").toBool());
        QVERIFY(!javascript(preview, "document.querySelector('article a').getAttribute('href').startsWith('javascript:')").toBool());
    }

    void viewModeKeepsScrollPosition_data() { // NOLINT(readability-identifier-naming): Qt Test data-provider convention.
        QTest::addColumn<QString>("fromMode");
        QTest::addColumn<QString>("toMode");
        QTest::newRow("edit-to-preview") << QString("editMode") << QString("previewMode");
        QTest::newRow("preview-to-edit") << QString("previewMode") << QString("editMode");
        QTest::newRow("edit-to-split") << QString("editMode") << QString("splitMode");
        QTest::newRow("split-to-edit") << QString("splitMode") << QString("editMode");
        QTest::newRow("preview-to-split") << QString("previewMode") << QString("splitMode");
        QTest::newRow("split-to-preview") << QString("splitMode") << QString("previewMode");
    }

    void viewModeKeepsScrollPosition() {
        QFETCH(QString, fromMode);
        QFETCH(QString, toMode);
        NotebookStore fixture;
        Notebook book{"mode-scroll-book", "Scroll", "normal", {}, {}, fixturePath("mode-scroll-book")};
        QString text;
        for (int i = 0; i < 80; ++i) text += QStringLiteral("## Heading %1\n\nParagraph %1\n\n").arg(i);
        book.notes.append({"mode-scroll-note", {}, "Scroll.md", text, false, {}});
        fixture.notebooks.append(book);
        QString error;
        QVERIFY2(fixture.save(&error), qPrintable(error));
        MainWindow window;
        window.resize(1400, 700);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto *tree = window.findChild<QTreeWidget *>("noteTree");
        openTreeNote(tree, tree->topLevelItem(0));
        auto *preview = window.findChild<MarkdownPreview *>("preview");
        auto *editor = window.findChild<MarkdownEditor *>("markdownEditor");
        QTest::mouseClick(window.findChild<QPushButton *>(fromMode), Qt::LeftButton);
        if (fromMode != "editMode") {
            QTRY_COMPARE_WITH_TIMEOUT(preview->renderedRevision(), preview->revision(), 30000);
        }
        QTest::qWait(100);
        QTextCursor selection(editor->document()->findBlockByNumber(fromMode == "previewMode" ? 2 : 160));
        selection.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, 3);
        editor->getTextEdit()->setTextCursor(selection);
        if (fromMode == "previewMode") {
            QSignalSpy lines(preview, &MarkdownPreview::sourceLineChanged);
            javascript(*preview, "window.scrollTo(0, Math.ceil(document.querySelector('h2[data-source-line=\"160\"]').getBoundingClientRect().top + scrollY))");
            QTRY_VERIFY(!lines.isEmpty());
            QCOMPARE(lines.last().first().toInt(), 160);
        } else {
            editor->scrollToLine(160, false);
            QTRY_COMPARE(editor->getTopLine(), 160);
        }
        const auto cursor = editor->getTextEdit()->textCursor();
        QVERIFY(cursor.hasSelection());
        QTest::mouseClick(window.findChild<QPushButton *>(toMode), Qt::LeftButton);
        if (toMode != "editMode") {
            QTRY_COMPARE_WITH_TIMEOUT(preview->renderedRevision(), preview->revision(), 30000);
            QTRY_VERIFY(javascript(*preview, "Math.abs(document.querySelector('h2[data-source-line=\"160\"]').getBoundingClientRect().top) < 2").toBool());
        }
        if (toMode != "previewMode") QTRY_COMPARE(editor->getTopLine(), 160);
        QCOMPARE(editor->getTextEdit()->textCursor().position(), cursor.position());
        QCOMPARE(editor->getTextEdit()->textCursor().anchor(), cursor.anchor());
    }

    void splitPreviewScrollsBothWays() {
        NotebookStore fixture;
        Notebook book{"scroll-book", "Scroll", "normal", {}, {}, fixturePath("scroll-book")};
        QString text;
        for (int i = 0; i < 80; ++i) text += QStringLiteral("## Heading %1\n\nParagraph %1\n\n").arg(i);
        book.notes.append({"scroll-note", {}, "Scroll.md", text, false, {}});
        fixture.notebooks.append(book);
        QString error;
        QVERIFY2(fixture.save(&error), qPrintable(error));
        MainWindow window;
        window.resize(1400, 700);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto *tree = window.findChild<QTreeWidget *>("noteTree");
        openTreeNote(tree, tree->topLevelItem(0));
        QTest::mouseClick(window.findChild<QPushButton *>("splitMode"), Qt::LeftButton);
        auto *preview = window.findChild<MarkdownPreview *>("preview");
        auto *editor = window.findChild<MarkdownEditor *>("markdownEditor");
        QTRY_COMPARE_WITH_TIMEOUT(preview->renderedRevision(), preview->revision(), 30000);
        editor->scrollToLine(160, false);
        QTRY_VERIFY(javascript(*preview, "window.scrollY > 1000").toBool());
        QTest::qWait(100);
        javascript(*preview, "window.scrollTo(0,0)");
        QTRY_VERIFY(editor->getTopLine() <= 2);
        window.grab().save("split-preview.png");
    }

    void framelessResizeCursors_data() { // NOLINT(readability-identifier-naming): Qt Test data-provider convention.
        QTest::addColumn<QPoint>("position");
        QTest::addColumn<Qt::CursorShape>("cursor");
        QTest::newRow("left") << QPoint(0, 1) << Qt::SizeHorCursor;
        QTest::newRow("right") << QPoint(2, 1) << Qt::SizeHorCursor;
        QTest::newRow("top") << QPoint(1, 0) << Qt::SizeVerCursor;
        QTest::newRow("bottom") << QPoint(1, 2) << Qt::SizeVerCursor;
        QTest::newRow("top-left") << QPoint(0, 0) << Qt::SizeFDiagCursor;
        QTest::newRow("top-right") << QPoint(2, 0) << Qt::SizeBDiagCursor;
        QTest::newRow("bottom-left") << QPoint(0, 2) << Qt::SizeBDiagCursor;
        QTest::newRow("bottom-right") << QPoint(2, 2) << Qt::SizeFDiagCursor;
    }

    void framelessResizeCursors() {
        QFETCH(QPoint, position);
        QFETCH(Qt::CursorShape, cursor);
        MainWindow window;
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        for (const QSize size: {QSize(1280, 800), QSize(480, 600)}) {
            window.resize(size);
            const QPoint point(position.x() * (window.width() - 1) / 2,
                               position.y() * (window.height() - 1) / 2);
            auto *target = window.childAt(point);
            QVERIFY(target);
            QTest::mouseMove(&window, point);
            QCOMPARE(target->cursor().shape(), cursor);
        }
        auto *header = window.findChild<QWidget *>("header");
        auto *toggleLeft = window.findChild<QPushButton *>("toggleLeft");
        const auto point = header->mapTo(&window, QPoint(toggleLeft->geometry().right() + 5, header->height() / 2));
        QTest::mouseMove(&window, point);
        QCOMPARE(window.childAt(point), header);
        QCOMPARE(window.childAt(point)->cursor().shape(), Qt::ArrowCursor);
        QCOMPARE(window.findChild<QLineEdit *>("search")->cursor().shape(), Qt::IBeamCursor);
        QCOMPARE(window.findChild<QPushButton *>("windowClose")->cursor().shape(), Qt::PointingHandCursor);
    }

    void framelessResizeWindowStates() {
        MainWindow window;
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto *corner = window.childAt(QPoint(0, 0));
        QVERIFY(corner);
        QCOMPARE(corner->cursor().shape(), Qt::SizeFDiagCursor);
        auto *maximize = window.findChild<QPushButton *>("windowMaximize");
        QTest::mouseClick(maximize, Qt::LeftButton);
        QTRY_VERIFY(window.isMaximized());
        QVERIFY(!corner->isVisible());
        QTest::mouseClick(maximize, Qt::LeftButton);
        QTRY_VERIFY(!window.isMaximized());
        QVERIFY(corner->isVisible());
        window.showFullScreen();
        QTRY_VERIFY(window.isFullScreen());
        QVERIFY(!corner->isVisible());
        window.showNormal();
        QTRY_VERIFY(!window.isFullScreen());
        QVERIFY(corner->isVisible());
        QCOMPARE(window.childAt(QPoint(0, 0)), corner);
    }

    void sessionRestoreRepro() {
        NotebookStore fixture;
        Notebook normal{"normal", "测试笔记本", "normal", {}, {}, fixturePath("normal")};
        normal.notes.append({"note-a", {}, "笔记 A", "内容 A", false, {}});
        normal.notes.append({"note-b", {}, "笔记 B", "内容 B", false, {}});
        fixture.notebooks.append(normal);
        QString error;
        QVERIFY2(fixture.save(&error), qPrintable(error));
        {
            MainWindow window;
            window.showMaximized();
            QVERIFY(QTest::qWaitForWindowExposed(&window));
            auto *tree = window.findChild<QTreeWidget *>("noteTree");
            openTreeNote(tree, tree->topLevelItem(0));
            openTreeNote(tree, tree->topLevelItem(1));
            QCOMPARE(window.findChild<QTabBar *>("noteTabs")->count(), 2);
            QTest::qWait(2000);
            QVERIFY(window.grab().save("/tmp/qsnote-opened.png"));
            auto *tabs1 = window.findChild<QTabBar *>("noteTabs");
            qDebug("opened: tabs=%d visible=%d geo=%dx%d@%d,%d sizeHint=%dx%d", tabs1->count(), tabs1->isVisible(),
                   tabs1->width(), tabs1->height(), tabs1->x(), tabs1->y(), tabs1->sizeHint().width(), tabs1->sizeHint().height());
            window.close();
        }
        {
            MainWindow window;
            window.showMaximized();
            QVERIFY(QTest::qWaitForWindowExposed(&window));
            auto *tabs = window.findChild<QTabBar *>("noteTabs");
            QCOMPARE(tabs->count(), 2);
            QVERIFY(tabs->isVisible());
            qDebug("restored: tabs=%d visible=%d geo=%dx%d@%d,%d sizeHint=%dx%d", tabs->count(), tabs->isVisible(),
                   tabs->width(), tabs->height(), tabs->x(), tabs->y(), tabs->sizeHint().width(), tabs->sizeHint().height());
            QVERIFY(window.grab().save("/tmp/qsnote-restored.png"));
            window.close();
        }
    }

    void emptyAndResponsive() {
        MainWindow window;
        window.resize(1440, 900);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QCOMPARE(window.findChild<QComboBox *>("notebooks")->count(), 0);
        QCOMPARE(window.findChild<QTreeWidget *>("noteTree")->topLevelItemCount(), 0);
        QCOMPARE(window.findChild<QStackedWidget *>("editorPages")->currentIndex(), 0);
        QVERIFY(!window.findChild<QWidget *>("toolbar")->isVisible());
        QVERIFY(!window.findChild<QPushButton *>("siteConfig")->isVisible());
        QTest::qWait(30);
        auto *search = window.findChild<QLineEdit *>("search");
        QCOMPARE(search->minimumWidth(), 320);
        window.grab().save("empty-wide.png");
        for (const QSize size: {QSize(1024, 700), QSize(720, 540), QSize(480, 600), QSize(360, 640)}) {
            window.resize(size);
            QTest::qWait(30);
            QCOMPARE(window.size(), size.expandedTo(window.minimumSize()));
            QVERIFY(search->width() >= 320);
            auto *center = window.findChild<QWidget *>("center");
            QVERIFY(center->width() > 200);
            QVERIFY(center->height() > 200);
        }
        QVERIFY(!window.findChild<QWidget *>("rightPanel")->isVisible());
        QTest::qWait(30);
        window.grab().save("empty-narrow.png");
        window.resize(1440, 900);
        QTest::qWait(30);
        QVERIFY(window.findChild<QWidget *>("leftPanel")->isVisible());
        QVERIFY(window.findChild<QWidget *>("rightPanel")->isVisible());
        QTest::mouseClick(window.findChild<QPushButton *>("toggleLeft"), Qt::LeftButton);
        QVERIFY(!window.findChild<QWidget *>("leftPanel")->isVisible());
        QTest::keyClick(&window, Qt::Key_F, Qt::ControlModifier);
        QVERIFY(window.findChild<QWidget *>("leftPanel")->isVisible());
        QVERIFY(window.findChild<QLineEdit *>("search")->hasFocus());
    }

    void createAllNotebookTypes() {
        MainWindow window;
        window.resize(1280, 800);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        for (int type = 0; type < 3; ++type) {
            QTimer::singleShot(30, &window, [type] {
                auto *dialog = QApplication::activeModalWidget();
                QVERIFY(dialog);
                dialog->findChild<QLineEdit *>("notebookName")->setText(QStringLiteral("用户笔记 %1").arg(type));
                dialog->findChild<QComboBox *>("notebookType")->setCurrentIndex(type);
                dialog->grab().save(QStringLiteral("new-notebook-%1.png").arg(type));
                QTest::mouseClick(dialog->findChild<QPushButton *>("confirmNotebook"), Qt::LeftButton);
            });
            QTest::mouseClick(window.findChild<QPushButton *>("newNotebook"), Qt::LeftButton);
            QCOMPARE(window.findChild<QComboBox *>("notebooks")->count(), type + 1);
            QCOMPARE(window.findChild<QTreeWidget *>("noteTree")->topLevelItemCount(), 0);
            QCOMPARE(window.findChild<QPushButton *>("siteConfig")->isVisible(), type == 1);
            QCOMPARE(window.findChild<QPushButton *>("exportNotebook")->isVisible(), type != 0);
        }
        NotebookStore reloaded;
        QString error;
        QVERIFY2(reloaded.load(&error), qPrintable(error));
        QCOMPARE(reloaded.notebooks.size(), 3);
        QCOMPARE(reloaded.notebooks.at(1).config["commentPlatform"].toString(), QString("none"));
        for (const auto &book: reloaded.notebooks) QVERIFY(book.notes.isEmpty());
    }

    void editingSearchTabsAndOutline() {
        NotebookStore fixture;
        Notebook normal{"normal", "测试笔记本", "normal", {}, {}, fixturePath("normal")};
        normal.notes.append({"note-a", {}, "笔记 A.md", "一级标题\n===\n\n正文\n\n## 二级标题\n\n```\n# 代码内不是标题\n```\n", false, {}});
        normal.notes.append({"note-b", {}, "笔记 B.md", "# 另一篇\n\n可搜索的正文", false, {}});
        fixture.notebooks.append(normal);
        QString error;
        QVERIFY2(fixture.save(&error), qPrintable(error));
        MainWindow window;
        window.resize(1440, 900);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto *tree = window.findChild<QTreeWidget *>("noteTree");
        auto *tabs = window.findChild<QTabBar *>("noteTabs");
        auto *outline = window.findChild<QTreeWidget *>("outline");
        auto clickNote = [tree](int index) {
            openTreeNote(tree, tree->topLevelItem(index));
        };
        clickNote(0);
        auto *editor = window.findChild<vte::VTextEdit *>("editor");
        QVERIFY(editor);
        QCOMPARE(tabs->count(), 1);
        QTRY_COMPARE(outline->topLevelItemCount(), 1);
        QCOMPARE(outline->topLevelItem(0)->childCount(), 1);
        editor->moveCursor(QTextCursor::End);
        editor->insertPlainText("\n用户新写的内容");
        clickNote(1);
        QCOMPARE(tabs->count(), 2);
        tabs->setCurrentIndex(0);
        QTRY_COMPARE(outline->topLevelItemCount(), 1);
        QCOMPARE(outline->topLevelItem(0)->text(0), QString("一级标题"));
        QVERIFY(editor->toPlainText().contains("用户新写的内容"));
        editor->moveCursor(QTextCursor::End);
        editor->insertPlainText("粗体");
        auto selection = editor->textCursor();
        selection.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor, 2);
        editor->setTextCursor(selection);
        QTest::mouseClick(window.findChild<QPushButton *>("toolB"), Qt::LeftButton);
        QVERIFY(editor->toPlainText().contains("**粗体**"));
        QTest::mouseClick(window.findChild<QPushButton *>("previewMode"), Qt::LeftButton);
        QCOMPARE(window.findChild<QStackedWidget *>("editorPages")->currentIndex(), 1);
        auto *preview = window.findChild<MarkdownPreview *>("preview");
        QTRY_COMPARE_WITH_TIMEOUT(preview->renderedRevision(), preview->revision(), 30000);
        QVERIFY(javascript(*preview, "document.body.textContent.includes('一级标题')").toBool());
        QVERIFY(!editor->isVisible());
        QTest::keyClick(&window, Qt::Key_F, Qt::ControlModifier);
        auto *readingFind = preview->findChild<QDialog *>("previewFind");
        QTRY_VERIFY(readingFind && readingFind->isVisible());
        QVERIFY(window.findChild<QPushButton *>("previewMode")->isChecked());
        readingFind->reject();
        QTest::mouseClick(window.findChild<QPushButton *>("splitMode"), Qt::LeftButton);
        QVERIFY(editor->isVisible());
        QVERIFY(preview->isVisible());
        editor->setFocus();
        QTimer::singleShot(30, &window, [] {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            QVERIFY(dialog && dialog->objectName() == "findReplace");
            dialog->accept();
        });
        QTest::keyClick(&window, Qt::Key_F, Qt::ControlModifier);
        QVERIFY(window.findChild<QPushButton *>("splitMode")->isChecked());
        editor->moveCursor(QTextCursor::End);
        editor->insertPlainText("\n\nSPLIT_UPDATE");
        QTRY_VERIFY(javascript(*preview, "document.body.textContent.includes('SPLIT_UPDATE')").toBool());
        editor->insertPlainText("\n\n- [ ] Pending task\n");
        QTRY_VERIFY(javascript(*preview, "!!document.querySelector('input.task-list-item-checkbox')").toBool());
        javascript(*preview, "document.querySelector('input.task-list-item-checkbox').click()");
        QTRY_VERIFY(editor->toPlainText().contains("- [x] Pending task"));
        editor->undo();
        QVERIFY(editor->toPlainText().contains("- [ ] Pending task"));
        QTRY_VERIFY(preview->matchesDocument(editor->toPlainText(), "note-a"));
        QTRY_COMPARE(preview->renderedRevision(), preview->revision());
        editor->moveCursor(QTextCursor::Start);
        editor->insertPlainText("Prefix\n");
        const auto changedSource = editor->toPlainText();
        javascript(*preview, "document.querySelector('input.task-list-item-checkbox').click()");
        QTest::qWait(250);
        QCOMPARE(editor->toPlainText(), changedSource);
        QTest::mouseClick(window.findChild<QPushButton *>("previewMode"), Qt::LeftButton);
        QTest::qWait(30);
        window.grab().save("preview-wide.png");
        QTest::mouseClick(window.findChild<QPushButton *>("editMode"), Qt::LeftButton);
        QTest::qWait(30);
        window.grab().save("editor-wide.png");
        window.resize(480, 600);
        QTest::qWait(30);
        QCOMPARE(window.size(), QSize(480, 600).expandedTo(window.minimumSize()));
        QVERIFY(editor->height() > 150);
        QTest::qWait(30);
        window.grab().save("editor-narrow.png");
        window.resize(1440, 900);
        auto *search = window.findChild<QLineEdit *>("search");
        search->setText("可搜索");
        QCOMPARE(tree->topLevelItemCount(), 1);
        QCOMPARE(tree->topLevelItem(0)->data(0, Qt::UserRole).toString(), QString("note-b"));
        QTest::qWait(40);
        window.grab().save("search-results.png");
        NotebookStore reloaded;
        QTRY_VERIFY_WITH_TIMEOUT(reloaded.load(&error) && reloaded.note("note-a") &&
                                 reloaded.note("note-a")->content.contains("用户新写的内容"), 7000);
        emit tabs->tabCloseRequested(0);
        QCOMPARE(tabs->count(), 1);
        emit tabs->tabCloseRequested(0);
        QCOMPARE(window.findChild<QStackedWidget *>("editorPages")->currentIndex(), 0);
    }

    void independentTabSessions() {
        NotebookStore fixture;
        Notebook book{"tabs-book", "Tabs", "normal", {}, {}, fixturePath("tabs-book")};
        QString originalA = "# A\n";
        for (int i = 0; i < 100; ++i) originalA += QStringLiteral("Line %1\n").arg(i);
        const QString originalB = "# B\nSecond note";
        book.notes.append({"tabs-a", {}, "A.md", originalA, false, {}});
        book.notes.append({"tabs-b", {}, "B.md", originalB, false, {}});
        fixture.notebooks.append(book);
        QString error;
        QVERIFY2(fixture.save(&error), qPrintable(error));
        MainWindow window;
        window.resize(1400, 800);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto *tree = window.findChild<QTreeWidget *>("noteTree");
        auto *tabs = window.findChild<QTabBar *>("noteTabs");
        auto open = [tree](int index) {
            openTreeNote(tree, tree->topLevelItem(index));
        };
        open(0);
        auto *a = window.findChild<vte::VTextEdit *>("editor");
        QVERIFY(a);
        QTest::qWait(50);
        a->moveCursor(QTextCursor::End);
        a->insertPlainText("A edit");
        QTextCursor selection(a->document()->findBlockByNumber(30));
        selection.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, 5);
        a->setTextCursor(selection);
        a->verticalScrollBar()->setValue(300);
        const int scroll = a->verticalScrollBar()->value();
        QVERIFY(scroll > 0);
        open(1);
        auto *b = window.findChild<vte::VTextEdit *>("editor");
        QVERIFY(b && b != a);
        QVERIFY(!a->isVisible());
        b->moveCursor(QTextCursor::End);
        b->insertPlainText(" B edit");
        tabs->setCurrentIndex(0);
        QCOMPARE(window.findChild<vte::VTextEdit *>("editor"), a);
        QCOMPARE(a->textCursor().anchor(), selection.anchor());
        QCOMPARE(a->textCursor().position(), selection.position());
        QTRY_COMPARE(a->verticalScrollBar()->value(), scroll);
        a->undo();
        QCOMPARE(a->toPlainText(), originalA);
        QCOMPARE(b->toPlainText(), originalB + " B edit");
        a->redo();
        QCOMPARE(a->toPlainText(), originalA + "A edit");
        tabs->setCurrentIndex(1);
        b->undo();
        QCOMPARE(b->toPlainText(), originalB);
        b->redo();
        QCOMPARE(b->toPlainText(), originalB + " B edit");
        QTest::mouseClick(window.findChild<QPushButton *>("splitMode"), Qt::LeftButton);
        tabs->setCurrentIndex(0);
        QVERIFY(window.findChild<QPushButton *>("editMode")->isChecked());
        tabs->setCurrentIndex(1);
        QVERIFY(window.findChild<QPushButton *>("splitMode")->isChecked());
        auto *preview = window.findChild<MarkdownPreview *>("preview");
        QTRY_COMPARE_WITH_TIMEOUT(preview->renderedRevision(), preview->revision(), 30000);
        QVERIFY(preview->matchesDocument(b->toPlainText(), "tabs-b"));
        auto *options = window.findChild<QPushButton *>("editorOptions")->menu();
        QVERIFY(options);
        QAction *wrap = nullptr;
        for (auto *action: options->actions()) if (action->text() == "自动换行") wrap = action;
        QVERIFY(wrap);
        wrap->trigger();
        QCOMPARE(window.findChild<MarkdownEditor *>("markdownEditor")->getConfig().m_wrapMode, vte::NoWrap);
        auto *hiddenEditor = window.findChild<MarkdownEditor *>("inactiveMarkdownEditor");
        QCOMPARE(hiddenEditor->getConfig().m_wrapMode, vte::NoWrap);
        QCOMPARE(a->toPlainText(), originalA + "A edit");
        QCOMPARE(b->toPlainText(), originalB + " B edit");
        auto *hiddenMenu = hiddenEditor->createEditorMenu(hiddenEditor);
        QMetaObject::invokeMethod(hiddenMenu, "aboutToShow");
        for (auto *action: hiddenMenu->actions()) {
            if (action->text() == "自动换行") QVERIFY(!action->isChecked());
        }
        // A delayed formatter can update a hidden document. Its writeback must
        // still target A, rather than whichever tab is currently active.
        a->moveCursor(QTextCursor::End);
        a->insertPlainText("\nHidden A");
        QTest::keyClick(&window, Qt::Key_S, Qt::ControlModifier);
        NotebookStore saved;
        QVERIFY2(saved.load(&error), qPrintable(error));
        QCOMPARE(saved.note("tabs-a")->content, a->toPlainText());
        QCOMPARE(saved.note("tabs-b")->content, b->toPlainText());
        const QPointer<vte::VTextEdit> closedA(a);
        emit tabs->tabCloseRequested(0);
        QVERIFY(closedA.isNull());
        QCOMPARE(window.findChild<vte::VTextEdit *>("editor"), b);
        QVERIFY(window.findChild<QPushButton *>("splitMode")->isChecked());
        auto *documentEditor = window.findChild<MarkdownEditor *>("markdownEditor");
        documentEditor->setInputMode(vte::ViMode);
        QTest::keyClicks(b, ":");
        const QPointer<vte::VTextEdit> closedB(b);
        emit tabs->tabCloseRequested(0);
        QVERIFY(closedB.isNull());
        QCOMPARE(tabs->count(), 0);
        open(0);
        auto *reopened = window.findChild<vte::VTextEdit *>("editor");
        QVERIFY(reopened);
        QCOMPARE(reopened->toPlainText(), saved.note("tabs-a")->content);
        QVERIFY(!reopened->document()->isUndoAvailable());
    }

    void notebookContextActionsAndChapterHierarchy() {
        NotebookStore fixture;
        fixture.notebooks.append({"normal", "操作验证", "normal", {}, {}, fixturePath("normal-ops")});
        fixture.notebooks.append({"ebook", "章节验证", "ebook", {}, {}, fixturePath("ebook-ops")});
        QString error;
        QVERIFY2(fixture.save(&error), qPrintable(error));
        MainWindow window;
        window.resize(1280, 800);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto *tree = window.findChild<QTreeWidget *>("noteTree");
        auto runAction = [&window, tree](QTreeWidgetItem *item, const QString &actionText, const QString &input = QString()) {
            QTimer::singleShot(20, &window, [actionText, input] {
                auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
                QVERIFY(menu);
                QAction *action = nullptr;
                for (auto *candidate: menu->actions()) {
                    if (candidate->text() == actionText) action = candidate;
                }
                if (!action) {
                    menu->close();
                    QFAIL("Missing context action");
                }
                if (!input.isNull()) {
                    QTimer::singleShot(20, menu, [input] {
                        auto *dialog = qobject_cast<QInputDialog *>(QApplication::activeModalWidget());
                        QVERIFY(dialog);
                        dialog->setTextValue(input);
                        dialog->accept();
                    });
                }
                QTest::mouseClick(menu, Qt::LeftButton, Qt::NoModifier, menu->actionGeometry(action).center());
            });
            const auto point = item ? tree->visualItemRect(item).center() : QPoint(20, tree->height() - 20);
            emit tree->customContextMenuRequested(point);
        };
        runAction(nullptr, "新建文件夹", "目录");
        QCOMPARE(tree->topLevelItemCount(), 1);
        runAction(tree->topLevelItem(0), "新建笔记", "新笔记");
        QCOMPARE(tree->topLevelItem(0)->childCount(), 1);
        QCOMPARE(window.findChild<QTabBar *>("noteTabs")->count(), 1);
        QCOMPARE(window.findChild<vte::VTextEdit *>("editor")->toPlainText(), QString());
        runAction(tree->topLevelItem(0)->child(0), "重命名", "重命名.md");
        QCOMPARE(tree->topLevelItem(0)->child(0)->text(0), QString("重命名.md"));
        QCOMPARE(window.findChild<QTabBar *>("noteTabs")->tabText(0), QString("重命名.md"));
        window.findChild<QComboBox *>("notebooks")->setCurrentIndex(1);
        runAction(nullptr, "添加章节", "第一章");
        runAction(nullptr, "添加章节", "第二章");
        QCOMPARE(tree->topLevelItemCount(), 2);
        runAction(tree->topLevelItem(1), "缩进为子章节");
        QCOMPARE(tree->topLevelItemCount(), 1);
        QCOMPARE(tree->topLevelItem(0)->childCount(), 1);
        runAction(tree->topLevelItem(0)->child(0), "提升层级");
        QCOMPARE(tree->topLevelItemCount(), 2);
        runAction(tree->topLevelItem(1), "上移");
        QCOMPARE(tree->topLevelItem(0)->text(0), QString("第二章"));
        runAction(tree->topLevelItem(0), "添加子章节", "子章节");
        QCOMPARE(tree->topLevelItem(0)->childCount(), 1);
        QTest::qWait(30);
        window.grab().save("ebook-wide.png");
        runAction(tree->topLevelItem(0)->child(0), "提升层级");
        QCOMPARE(tree->topLevelItemCount(), 3);
        QCOMPARE(tree->topLevelItem(1)->text(0), QString("子章节"));
        NotebookStore reloaded;
        QVERIFY2(reloaded.load(&error), qPrintable(error));
        QCOMPARE(reloaded.notebooks[0].notes.size(), 2);
        QCOMPARE(reloaded.notebooks[1].notes.size(), 3);
    }

    void closeAndReopenNotebook() {
        NotebookStore fixture;
        Notebook first{"book-a", "笔记本甲", "normal", {}, {}, fixturePath("book-a")};
        first.notes.append({"note-x", {}, "笔记X.md", "内容X", false, {}});
        fixture.notebooks.append(first);
        fixture.notebooks.append({"book-b", "笔记本乙", "normal", {}, {}, fixturePath("book-b")});
        QString error;
        QVERIFY2(fixture.save(&error), qPrintable(error));
        MainWindow window;
        window.resize(1280, 800);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto *notebooks = window.findChild<QComboBox *>("notebooks");
        auto *tree = window.findChild<QTreeWidget *>("noteTree");
        auto *tabs = window.findChild<QTabBar *>("noteTabs");
        QCOMPARE(notebooks->count(), 2);
        openTreeNote(tree, tree->topLevelItem(0));
        QCOMPARE(tabs->count(), 1);
        auto triggerAction = [](const QString &text) {
            auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
            QVERIFY(menu);
            QAction *match = nullptr;
            for (auto *action: menu->actions()) {
                if (action->text() == text) match = action;
            }
            if (!match) {
                menu->close();
                QFAIL("Missing context action");
            }
            match->trigger();
        };
        QTimer::singleShot(20, &window, [triggerAction] {
            // 触发动作会同步进入模态循环，对话框处理必须先注册。
            QTimer::singleShot(20, qApp, [] {
                auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
                QVERIFY(box);
                box->button(QMessageBox::Yes)->click();
            });
            triggerAction("关闭笔记本");
        });
        emit tree->customContextMenuRequested(QPoint(20, tree->height() - 20));
        QTRY_COMPARE(notebooks->count(), 1);
        QCOMPARE(tabs->count(), 0);
        QCOMPARE(notebooks->currentText(), QString("笔记本乙"));
        // 关闭只移除登记，磁盘目录与索引保留
        QVERIFY(QFile::exists(fixturePath("book-a") + "/_index.json"));
        QVERIFY(QFile::exists(fixturePath("book-a") + "/笔记X.md"));
        NotebookStore reloaded;
        QVERIFY2(reloaded.load(&error), qPrintable(error));
        QCOMPARE(reloaded.notebooks.size(), 1);
        QCOMPARE(reloaded.notebooks.first().id, QString("book-b"));
        // 存储接口可直接恢复被关闭的笔记本
        Notebook restored;
        QVERIFY2(NotebookStore::readNotebook(fixturePath("book-a"), restored, &error), qPrintable(error));
        QCOMPARE(restored.id, QString("book-a"));
        QCOMPARE(restored.name, QString("笔记本甲"));
        QCOMPARE(restored.notes.size(), 1);
        QCOMPARE(restored.notes.first().content, QString("内容X"));
        // 通过界面重新打开
        QTimer::singleShot(20, &window, [triggerAction] {
            QTimer::singleShot(30, qApp, [] {
                auto *dialog = qobject_cast<QFileDialog *>(QApplication::activeModalWidget());
                QVERIFY(dialog);
                dialog->setDirectory(fixturePath("book-a"));
                QTimer::singleShot(80, dialog, [dialog] {
                    dialog->findChild<QLineEdit *>("fileNameEdit")->setText(fixturePath("book-a"));
                    QMetaObject::invokeMethod(dialog, "accept");
                });
            });
            triggerAction("打开笔记本…");
        });
        emit tree->customContextMenuRequested(QPoint(20, tree->height() - 20));
        QTRY_COMPARE(notebooks->count(), 2);
        QCOMPARE(notebooks->currentText(), QString("笔记本甲"));
        QCOMPARE(tree->topLevelItemCount(), 1);
        QCOMPARE(tree->topLevelItem(0)->text(0), QString("笔记X.md"));
    }

    void ebookExportOptionsAndUnsavedDocument() {
        NotebookStore fixture;
        fixture.notebooks.append({"ebook-options", "整本书", "ebook", {
            {"parent", {}, "父章节", "原始内容", false, {}},
            {"child", "parent", "子章节", "子章节正文", false, {}}
        }, {}, fixturePath("ebook-options")});
        QString error;
        QVERIFY2(fixture.save(&error), qPrintable(error));
        QTemporaryDir destination;
        MainWindow window;
        window.resize(1100, 800);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto *tree = window.findChild<QTreeWidget *>("noteTree");
        QVERIFY(tree);
        openTreeNote(tree, tree->topLevelItem(0));
        auto *editor = window.findChild<MarkdownEditor *>("markdownEditor");
        QVERIFY(editor);
        editor->setText("尚未等待自动保存的新内容");
        for (int format = 0; format < 4; ++format) {
            QTimer::singleShot(30, &window, [&destination, format] {
                auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
                QVERIFY(dialog);
                QCOMPARE(dialog->objectName(), QString("ebookExportDialog"));
                auto *scope = dialog->findChild<QComboBox *>("exportScope");
                QCOMPARE(scope->count(), 2);
                scope->setCurrentIndex(1);
                auto *formats = dialog->findChild<QComboBox *>("exportFormat");
                QCOMPARE(formats->count(), 4);
                formats->setCurrentIndex(format);
                auto *directory = dialog->findChild<QLineEdit *>("exportDirectory");
                auto *confirm = dialog->findChild<QPushButton *>("confirmExport");
                directory->setText(destination.path() + "/missing");
                QVERIFY(!confirm->isEnabled());
                directory->setText(destination.path());
                if (format == 2 && EbookExporter::chmCompiler().isEmpty()) {
                    QVERIFY(!confirm->isEnabled());
                    dialog->reject();
                    return;
                }
                QVERIFY(confirm->isEnabled());
                dialog->grab().save("ebook-export-dialog.png");
                confirm->click();
            });
            window.findChild<QPushButton *>("exportNotebook")->click();
        }
        QFile html(destination.filePath("父章节.html"));
        QVERIFY(html.open(QIODevice::ReadOnly));
        const auto content = QString::fromUtf8(html.readAll());
        QVERIFY(content.contains("尚未等待自动保存的新内容"));
        QVERIFY(!content.contains("子章节正文"));
        QVERIFY(QFileInfo::exists(destination.filePath("父章节.epub")));
        QVERIFY(QFileInfo::exists(destination.filePath("父章节.pdf")));
        if (!EbookExporter::chmCompiler().isEmpty()) QVERIFY(QFileInfo::exists(destination.filePath("父章节.chm")));
        const auto files = QDir(destination.path()).entryList(QDir::Files);
        QTimer::singleShot(30, &window, [&destination] {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            QCOMPARE(dialog->findChild<QLineEdit *>("exportDirectory")->text(), destination.path());
            dialog->reject();
        });
        window.findChild<QPushButton *>("exportNotebook")->click();
        QCOMPARE(QDir(destination.path()).entryList(QDir::Files), files);
    }

    void exportsContainUserContent() {
        QTemporaryDir remote;
        QVERIFY(remote.isValid());
        QCOMPARE(QProcess::execute("git", {"init", "--bare", remote.path()}), 0);
        NotebookStore fixture;
        fixture.notebooks.append({"site", "导出站点", "site", {{"post", {}, "文章", "# 用户文章\n\n正文", false, {}}}, {{"title", "用户站点"}, {"publishMethod", "github"}, {"githubRepository", remote.path()}}, fixturePath("site-export")});
        fixture.notebooks.append({"ebook", "导出书籍", "ebook", {{"chapter", {}, "章节", "# 用户章节\n\n章节正文", false, {}}}, {}, fixturePath("ebook-export")});
        QString error;
        QVERIFY2(fixture.save(&error), qPrintable(error));
        QTemporaryDir destination;
        QVERIFY(destination.isValid());
        MainWindow window;
        window.resize(1280, 800);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        for (int index = 0; index < 2; ++index) {
            window.findChild<QComboBox *>("notebooks")->setCurrentIndex(index);
            if (index == 1) QTimer::singleShot(30, &window, [&destination] {
                auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
                QVERIFY(dialog);
                dialog->findChild<QLineEdit *>("exportDirectory")->setText(destination.path());
                dialog->findChild<QComboBox *>("exportFormat")->setCurrentIndex(3);
                dialog->findChild<QPushButton *>("confirmExport")->click();
            });
            QTest::mouseClick(window.findChild<QPushButton *>("exportNotebook"), Qt::LeftButton);
        }
        const QDir output(destination.path());
        QProcess published;
        published.start("git", {"--git-dir", remote.path(), "show", "gh-pages:文章/index.html"});
        QVERIFY(published.waitForFinished());
        QCOMPARE(published.exitCode(), 0);
        QVERIFY(QString::fromUtf8(published.readAllStandardOutput()).contains("用户文章"));
        QFile book(output.filePath("导出书籍.html"));
        QVERIFY(book.open(QIODevice::ReadOnly));
        QVERIFY(QString::fromUtf8(book.readAll()).contains("章节正文"));
    }

    void sitePublishSettings() {
        NotebookStore fixture;
        fixture.notebooks.append({"site", "发布站点", "site", {}, {}, fixturePath("publish-settings")});
        QString error;
        QVERIFY2(fixture.save(&error), qPrintable(error));
        MainWindow window;
        window.resize(1280, 800);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QCOMPARE(window.findChild<QPushButton *>("exportNotebook")->text(), QString("发布站点"));
        const QList<QPair<QString, QStringList>> groups{
            {"github", {"githubRepository", "githubBranch"}},
            {"scp", {"sshHost", "sshPort", "sshUser", "sshDirectory", "sshIdentityFile"}}};
        QTimer::singleShot(30, &window, [&] {
            auto *dialog = QApplication::activeModalWidget();
            QVERIFY(dialog);
            auto *method = dialog->findChild<QComboBox *>("config_publishMethod");
            QVERIFY(method);
            QCOMPARE(method->currentData().toString(), QString());
            for (const auto &platform: {"", "github", "scp", "github", "scp"}) {
                method->setCurrentIndex(method->findData(platform));
                for (const auto &group: groups) {
                    for (const auto &key: group.second) {
                        auto *input = dialog->findChild<QLineEdit *>("config_" + key);
                        QVERIFY(input);
                        QCOMPARE(input->isVisible(), group.first == platform);
                        QCOMPARE(input->parentWidget()->findChild<QLabel *>()->isVisible(), group.first == platform);
                        input->setText("saved-" + key);
                        emit input->textEdited(input->text());
                    }
                }
            }
            dialog->findChild<QPushButton *>("closeSiteConfig")->click();
        });
        window.findChild<QPushButton *>("siteConfig")->click();
        Notebook restored;
        QVERIFY2(NotebookStore::readNotebook(fixturePath("publish-settings"), restored, &error), qPrintable(error));
        QCOMPARE(restored.config["publishMethod"].toString(), QString("scp"));
        QTimer::singleShot(30, &window, [&] {
            auto *dialog = QApplication::activeModalWidget();
            QVERIFY(dialog);
            QCOMPARE(dialog->findChild<QComboBox *>("config_publishMethod")->currentData().toString(), QString("scp"));
            for (const auto &group: groups) {
                for (const auto &key: group.second) {
                    auto *input = dialog->findChild<QLineEdit *>("config_" + key);
                    QCOMPARE(input->text(), "saved-" + key);
                    QCOMPARE(restored.config[key].toString(), "saved-" + key);
                    QCOMPARE(input->isVisible(), group.first == "scp");
                }
            }
            dialog->findChild<QPushButton *>("closeSiteConfig")->click();
        });
        window.findChild<QPushButton *>("siteConfig")->click();
        window.setFixedSize(420, 800);
        QTRY_COMPARE(window.findChild<QPushButton *>("exportNotebook")->text(), QString("发布"));
    }

    void siteMetadataConfigAndPreview_data() { // NOLINT(readability-identifier-naming): Qt Test data-provider convention.
        QTest::addColumn<QString>("theme");
        QTest::newRow("pure") << QString("pure");
        QTest::newRow("stellar") << QString("stellar");
    }

    void siteMetadataConfigAndPreview() {
        QFETCH(QString, theme);
        NotebookStore fixture;
        Notebook site{"site", "测试站点", "site", {}, {{"title", "用户的站点"}, {"categories", QJsonArray{"Updated"}}, {"domain", "https://example.com/blog/"}}, fixturePath("site")};
        site.notes.append({"post-a", {}, "文章标题", "# 文章标题\n\n## 内容\n\n真实输入的文字", false, {{"category", "分类一"}, {"tags", "标签一, 标签二"}, {"date", "2026-10-09"}, {"slug", "article"}}});
        fixture.notebooks.append(site);
        QString error;
        QVERIFY2(fixture.save(&error), qPrintable(error));
        MainWindow window;
        window.resize(1440, 900);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto *tree = window.findChild<QTreeWidget *>("noteTree");
        openTreeNote(tree, tree->topLevelItem(0)->child(0));
        QTimer::singleShot(30, &window, [&] {
            auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
            QVERIFY(menu);
            QAction *properties = nullptr;
            for (auto *action: menu->actions()) {
                if (action->text() == "属性更改") properties = action;
            }
            QVERIFY(properties);
            menu->hide();
            QTimer::singleShot(30, &window, [] {
                auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
                QVERIFY(dialog);
                auto *category = dialog->findChild<QComboBox *>();
                QVERIFY(category);
                category->setCurrentText("Updated");
                dialog->accept();
            });
            properties->trigger();
            menu->close();
        });
        const auto articlePosition = tree->visualItemRect(tree->topLevelItem(0)->child(0)).center();
        emit tree->customContextMenuRequested(articlePosition);
        QVERIFY(tree->topLevelItem(0)->text(0).contains("Updated"));
        window.findChild<QComboBox *>("groupBy")->setCurrentIndex(1);
        QCOMPARE(tree->topLevelItemCount(), 2);
        window.grab().save("site-wide.png");
        QTimer::singleShot(30, &window, [theme] {
            auto *dialog = QApplication::activeModalWidget();
            QVERIFY(dialog);
            QCOMPARE(dialog->objectName(), QString("siteConfigDialog"));
            auto *title = dialog->findChild<QLineEdit *>("config_title");
            title->setText("Edited title");
            emit title->textEdited(title->text());
            auto *comments = dialog->findChild<QComboBox *>("config_commentPlatform");
            QVERIFY(comments);
            QVERIFY(comments->findData("valine") >= 0);
            QCOMPARE(comments->currentData().toString(), QString("none"));
            for (const auto &platform: {"none", "valine", "gitalk", "disqus", "none", "valine"}) {
                comments->setCurrentIndex(comments->findData(platform));
                for (const auto &group: QList<QPair<QString, QStringList>>{
                         {"valine", {"valineAppId", "valineAppKey"}},
                         {"gitalk", {"commentOwner", "commentRepo", "clientId", "clientSecret"}},
                         {"disqus", {"disqus"}}}) {
                    for (const auto &key: group.second) {
                        auto *input = dialog->findChild<QLineEdit *>("config_" + key);
                        QVERIFY(input);
                        QCOMPARE(input->isVisible(), group.first == platform);
                        QCOMPARE(input->parentWidget()->findChild<QLabel *>()->isVisible(), group.first == platform);
                    }
                }
            }
            for (const auto &key: {"valineAppId", "valineAppKey"}) {
                auto *input = dialog->findChild<QLineEdit *>(QString("config_") + key);
                QVERIFY(input);
                QTest::keyClicks(input, QString("test-") + key);
            }
            auto *themes = dialog->findChild<QComboBox *>("config_theme");
            QVERIFY(themes);
            QCOMPARE(themes->currentData().toString(), QString("pure"));
            QVERIFY(themes->findData("stellar") >= 0);
            themes->setCurrentIndex(themes->findData(theme));
            QVERIFY(!dialog->findChild<QLineEdit *>("config_token"));
            dialog->grab().save("site-config.png");
            QTest::mouseClick(dialog->findChild<QPushButton *>("closeSiteConfig"), Qt::LeftButton);
        });
        QTest::mouseClick(window.findChild<QPushButton *>("siteConfig"), Qt::LeftButton);
        QTimer::singleShot(30, &window, [theme] {
            auto *dialog = QApplication::activeModalWidget();
            QVERIFY(dialog);
            QCOMPARE(dialog->findChild<QComboBox *>("config_theme")->currentData().toString(), theme);
            auto *comments = dialog->findChild<QComboBox *>("config_commentPlatform");
            QVERIFY(comments);
            QCOMPARE(comments->currentData().toString(), QString("valine"));
            for (const auto &key: {"valineAppId", "valineAppKey"}) {
                auto *input = dialog->findChild<QLineEdit *>(QString("config_") + key);
                QVERIFY(input);
                QVERIFY(input->isVisible());
                QCOMPARE(input->text(), QString("test-") + key);
            }
            comments->setCurrentIndex(comments->findData("none"));
            QTest::mouseClick(dialog->findChild<QPushButton *>("closeSiteConfig"), Qt::LeftButton);
        });
        QTest::mouseClick(window.findChild<QPushButton *>("siteConfig"), Qt::LeftButton);
        QTimer previewTimer;
        previewTimer.setInterval(100);
        connect(&previewTimer, &QTimer::timeout, &window, [&] {
            auto *dialog = QApplication::activeModalWidget();
            if (!dialog || dialog->objectName() != "sitePreviewDialog") return;
            previewTimer.stop();
            const auto closePreview = qScopeGuard([dialog] { QMetaObject::invokeMethod(dialog, "reject"); });
            auto *browser = dialog->findChild<QWebEngineView *>("siteBrowser");
            QVERIFY(browser);
            QSignalSpy loaded(browser, &QWebEngineView::loadFinished);
            if (browser->title().isEmpty()) QVERIFY(loaded.wait(15000));
            QVERIFY(browser->title().contains("Edited title"));
            QVERIFY(javascript(*browser, theme == "pure" ? "!!document.querySelector('body.main-center > header.header')" : "!!document.querySelector('.l_body .l_left')").toBool());
            QCOMPARE(browser->url().path(), QString("/blog/"));
            javascript(*browser, "fetch('/blog/search.json').then(r => window.searchIndexReady = r.ok)");
            QTRY_VERIFY_WITH_TIMEOUT(javascript(*browser, "window.searchIndexReady === true").toBool(), 10000);
            QVERIFY(javascript(*browser, "!!document.querySelector('a[href=\"/blog/archives\"], a[href=\"/blog/archives/\"]')").toBool());
            QVERIFY(javascript(*browser, QString("Array.from(document.styleSheets).some(s => s.href && s.href.includes('/css/%1.css') && s.cssRules.length > 0)").arg(theme == "pure" ? "style" : "main")).toBool());
            if (theme == "stellar") QTRY_VERIFY_WITH_TIMEOUT(javascript(*browser, "typeof searchFunc === 'function'").toBool(), 10000);
            if (theme == "pure") javascript(*browser, "document.querySelector('.search-form-input').click(); const input=document.querySelector('.ins-search-input'); input.value='真实输入'; input.dispatchEvent(new Event('input', {bubbles:true})); input.dispatchEvent(new KeyboardEvent('keyup', {bubbles:true}));");
            else javascript(*browser, "const input=document.querySelector('#search-input'); input.focus(); input.dispatchEvent(new FocusEvent('focusin', {bubbles:true})); input.value='真实输入'; input.dispatchEvent(new Event('input', {bubbles:true}));");
            QTRY_VERIFY2_WITH_TIMEOUT(javascript(*browser, "!!document.querySelector('.ins-section-container .ins-search-item[data-url=\"/blog/article/\"], #search-result a[href^=\"/blog/article/\"]')").toBool(), qPrintable(javascript(*browser, "JSON.stringify({active:document.activeElement.id,input:document.querySelector('#search-input')?._searchInitialized,cache:typeof searchCache === 'undefined' ? null : searchCache,root:typeof ctx === 'undefined' ? null : ctx.root,result:document.querySelector('#search-result')?.innerHTML})").toString()), 10000);
            browser->load(browser->url().resolved(QUrl("article/")));
            QVERIFY(loaded.wait(15000));
            QString text;
            QEventLoop loop;
            browser->page()->toPlainText([&](const QString &value) { text = value; loop.quit(); });
            loop.exec();
            QVERIFY(text.contains("真实输入的文字"));
            QTRY_VERIFY(javascript(*browser, "!!document.querySelector('h1.article-title, h1.text.title') && document.querySelector('h1.article-title, h1.text.title').textContent.trim() === '文章标题'").toBool());
            if (theme == "stellar") QVERIFY(javascript(*browser, "!!document.querySelector('.toc .toc-child .toc-link')").toBool());
            QTest::qWait(100);
            dialog->grab().save("site-preview-" + theme + ".png");
            dialog->resize(420, 800);
            QTRY_VERIFY(javascript(*browser, "window.innerWidth <= 420 && document.documentElement.scrollWidth <= window.innerWidth").toBool());
            QTest::qWait(100);
            dialog->grab().save("site-preview-" + theme + "-mobile.png");
            const auto menu = theme == "pure" ? ".navbar-toggle" : ".leftbar-toggle";
            QTRY_VERIFY(javascript(*browser, QString("document.querySelector('%1').getBoundingClientRect().width > 0").arg(menu)).toBool());
            javascript(*browser, QString("document.querySelector('%1').click()").arg(menu));
            QTRY_VERIFY(javascript(*browser, theme == "pure" ? "document.querySelector('#main-navbar').classList.contains('in')" : "document.querySelector('.l_body').hasAttribute('leftbar')").toBool());
            javascript(*browser, QString("document.querySelector('%1').click()").arg(menu));
            QTRY_VERIFY(javascript(*browser, theme == "pure" ? "!document.querySelector('#main-navbar').classList.contains('in')" : "!document.querySelector('.l_body').hasAttribute('leftbar')").toBool());
            QTest::mouseClick(dialog->findChild<QPushButton *>("closeSitePreview"), Qt::LeftButton);
        });
        previewTimer.start();
        QTest::mouseClick(window.findChild<QPushButton *>("sitePreview"), Qt::LeftButton);
        NotebookStore reloaded;
        QVERIFY2(reloaded.load(&error), qPrintable(error));
        QCOMPARE(reloaded.notebooks.first().config["title"].toString(), QString("Edited title"));
        QCOMPARE(reloaded.notebooks.first().config["theme"].toString(), theme);
        QCOMPARE(reloaded.notebooks.first().config["valineAppId"].toString(), QString("test-valineAppId"));
        QCOMPARE(reloaded.notebooks.first().config["valineAppKey"].toString(), QString("test-valineAppKey"));
        QCOMPARE(reloaded.note("post-a")->metadata["category"].toString(), QString("Updated"));
    }
};

// NOLINTEND(readability-convert-member-functions-to-static)
int main(int argc, char **argv) {
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QApplication app(argc, argv);
    QTemporaryDir settingsDirectory;
    if (!settingsDirectory.isValid()) return 1;
    QCoreApplication::setOrganizationName("qsnote-tests");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDirectory.path());
    UiTest tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "UiTest.moc"
