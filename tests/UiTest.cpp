#include "core/NotebookStore.h"
#include "ui/MainWindow.h"
#include "ui/MarkdownEditor.h"
#include "ui/MarkdownPreview.h"
#include <QWebEnginePage>
#include <QEventLoop>
#include <QPointer>
#include <QSignalSpy>
#include <QScrollBar>
#include <QTextBlock>
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
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <vtextedit/vtextedit.h>
#include <vtextedit/texteditorconfig.h>
#include <QPushButton>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTabBar>
#include <QTemporaryDir>
#include <QTest>
#include <QTextBrowser>
#include <QTimer>
#include <QTreeWidget>

namespace {
QString fixturePath(const QString &name) {
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/notebooks/" + name;
}
}// namespace

// Qt Test discovers non-static slots through the meta-object system.
// NOLINTBEGIN(readability-convert-member-functions-to-static)
class UiTest : public QObject {
    Q_OBJECT
    static QVariant javascript(MarkdownPreview &preview, const QString &script) {
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
        QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier, tree->visualItemRect(tree->topLevelItem(0)).center());
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
        window.grab().save("empty-wide.png");
        for (const QSize size: {QSize(1024, 700), QSize(720, 540), QSize(480, 600), QSize(360, 640)}) {
            window.resize(size);
            QTest::qWait(30);
            QCOMPARE(window.size(), size);
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
            QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier, tree->visualItemRect(tree->topLevelItem(index)).center());
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
        QCOMPARE(window.size(), QSize(480, 600));
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
        QTest::qWait(400);
        NotebookStore reloaded;
        QVERIFY2(reloaded.load(&error), qPrintable(error));
        QVERIFY(reloaded.note("note-a")->content.contains("用户新写的内容"));
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
            QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier, tree->visualItemRect(tree->topLevelItem(index)).center());
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
        QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier, tree->visualItemRect(tree->topLevelItem(0)).center());
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

    void exportsContainUserContent() {
        NotebookStore fixture;
        fixture.notebooks.append({"site", "导出站点", "site", {{"post", {}, "文章", "# 用户文章\n\n正文", false, {}}}, {{"title", "用户站点"}}, fixturePath("site-export")});
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
            QTimer::singleShot(30, &window, [&destination] {
                auto *dialog = qobject_cast<QFileDialog *>(QApplication::activeModalWidget());
                QVERIFY(dialog);
                dialog->setDirectory(destination.path());
                QTimer::singleShot(80, dialog, [dialog, &destination] {
                    dialog->findChild<QLineEdit *>("fileNameEdit")->setText(destination.path());
                    QMetaObject::invokeMethod(dialog, "accept");
                });
            });
            QTest::mouseClick(window.findChild<QPushButton *>("exportNotebook"), Qt::LeftButton);
        }
        const QDir output(destination.path());
        const auto sites = output.entryList({"qsnote-site-*"}, QDir::Dirs);
        const auto books = output.entryList({"qsnote-ebook-*"}, QDir::Dirs);
        QCOMPARE(sites.size(), 1);
        QCOMPARE(books.size(), 1);
        QFile article(output.filePath(sites.first() + "/post-post.html"));
        QVERIFY(article.open(QIODevice::ReadOnly));
        QVERIFY(QString::fromUtf8(article.readAll()).contains("用户文章"));
        QFile summary(output.filePath(books.first() + "/SUMMARY.md"));
        QVERIFY(summary.open(QIODevice::ReadOnly));
        QVERIFY(QString::fromUtf8(summary.readAll()).contains("[章节](chapter-chapter.md)"));
        QFile chapter(output.filePath(books.first() + "/chapter-chapter.md"));
        QVERIFY(chapter.open(QIODevice::ReadOnly));
        QCOMPARE(QString::fromUtf8(chapter.readAll()), QString("# 用户章节\n\n章节正文"));
    }

    void siteMetadataConfigAndPreview() {
        NotebookStore fixture;
        Notebook site{"site", "测试站点", "site", {}, {{"title", "用户的站点"}}, fixturePath("site")};
        site.notes.append({"post-a", {}, "文章标题", "# 文章标题\n\n## 内容\n\n真实输入的文字", false, {{"category", "分类一"}, {"tags", "标签一, 标签二"}, {"date", "2026-10-09"}, {"slug", "article"}}});
        fixture.notebooks.append(site);
        QString error;
        QVERIFY2(fixture.save(&error), qPrintable(error));
        MainWindow window;
        window.resize(1440, 900);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto *tree = window.findChild<QTreeWidget *>("noteTree");
        QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier, tree->visualItemRect(tree->topLevelItem(0)->child(0)).center());
        QVERIFY(window.findChild<QWidget *>("metadata")->isVisible());
        auto *category = window.findChild<QLineEdit *>("meta_category");
        category->setFocus();
        category->selectAll();
        QTest::keyClicks(category, "Updated");
        QVERIFY(tree->topLevelItem(0)->text(0).contains("Updated"));
        window.findChild<QComboBox *>("groupBy")->setCurrentIndex(1);
        QCOMPARE(tree->topLevelItemCount(), 2);
        QTest::qWait(30);
        window.grab().save("site-wide.png");
        window.resize(480, 600);
        QTest::qWait(30);
        QCOMPARE(window.size(), QSize(480, 600));
        QVERIFY(window.findChild<vte::VTextEdit *>("editor")->height() > 100);
        QTest::qWait(30);
        window.grab().save("site-narrow.png");
        window.resize(360, 640);
        QTest::qWait(30);
        QCOMPARE(window.size(), QSize(360, 640));
        QVERIFY(window.findChild<vte::VTextEdit *>("editor")->height() > 100);
        window.grab().save("site-mobile.png");
        window.resize(1440, 900);
        QTimer::singleShot(30, &window, [] {
            auto *dialog = QApplication::activeModalWidget();
            QVERIFY(dialog);
            QCOMPARE(dialog->objectName(), QString("siteConfigDialog"));
            auto *title = dialog->findChild<QLineEdit *>("config_title");
            title->setText("Edited title");
            emit title->textEdited(title->text());
            QVERIFY(dialog->findChild<QLineEdit *>("config_token")->echoMode() == QLineEdit::Password);
            dialog->grab().save("site-config.png");
            QTest::mouseClick(dialog->findChild<QPushButton *>("closeSiteConfig"), Qt::LeftButton);
        });
        QTest::mouseClick(window.findChild<QPushButton *>("siteConfig"), Qt::LeftButton);
        QTimer::singleShot(30, &window, [] {
            auto *dialog = QApplication::activeModalWidget();
            QVERIFY(dialog);
            auto *browser = dialog->findChild<QTextBrowser *>("siteBrowser");
            QVERIFY(browser->toPlainText().contains("Edited title"));
            emit browser->anchorClicked(QUrl("post-post-a.html"));
            QVERIFY(browser->toPlainText().contains("真实输入的文字"));
            dialog->grab().save("site-preview.png");
            QTest::mouseClick(dialog->findChild<QPushButton *>("closeSitePreview"), Qt::LeftButton);
        });
        QTest::mouseClick(window.findChild<QPushButton *>("sitePreview"), Qt::LeftButton);
        NotebookStore reloaded;
        QVERIFY2(reloaded.load(&error), qPrintable(error));
        QCOMPARE(reloaded.notebooks.first().config["title"].toString(), QString("Edited title"));
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
