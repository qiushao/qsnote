#include "core/NotebookStore.h"
#include "ui/MainWindow.h"
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTabBar>
#include <QTemporaryDir>
#include <QTest>
#include <QTextBrowser>
#include <QTimer>
#include <QTreeWidget>

// Qt Test discovers non-static slots through the meta-object system.
// NOLINTBEGIN(readability-convert-member-functions-to-static)
class UiTest : public QObject {
    Q_OBJECT
private slots:
    void init() {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(QDir().mkpath(QFileInfo(NotebookStore::storagePath()).absolutePath()));
        QFile::remove(NotebookStore::storagePath());
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
        Notebook normal{"normal", "测试笔记本", "normal", {}, {}};
        normal.notes.append({"note-a", {}, "笔记 A.md", "# 一级标题\n\n正文\n\n## 二级标题\n\n```\n# 代码内不是标题\n```\n", false, {}});
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
        auto *editor = window.findChild<QPlainTextEdit *>("editor");
        auto *outline = window.findChild<QTreeWidget *>("outline");
        auto clickNote = [tree](int index) {
            QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier, tree->visualItemRect(tree->topLevelItem(index)).center());
        };
        clickNote(0);
        QCOMPARE(tabs->count(), 1);
        QCOMPARE(outline->topLevelItemCount(), 1);
        QCOMPARE(outline->topLevelItem(0)->childCount(), 1);
        editor->moveCursor(QTextCursor::End);
        editor->insertPlainText("\n用户新写的内容");
        clickNote(1);
        QCOMPARE(tabs->count(), 2);
        tabs->setCurrentIndex(0);
        QVERIFY(editor->toPlainText().contains("用户新写的内容"));
        QTest::mouseClick(window.findChild<QPushButton *>("toolB"), Qt::LeftButton);
        QVERIFY(editor->toPlainText().contains("**粗体**"));
        QTest::mouseClick(window.findChild<QPushButton *>("previewMode"), Qt::LeftButton);
        QCOMPARE(window.findChild<QStackedWidget *>("editorPages")->currentIndex(), 2);
        QVERIFY(window.findChild<QTextBrowser *>("preview")->toPlainText().contains("一级标题"));
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

    void notebookContextActionsAndChapterHierarchy() {
        NotebookStore fixture;
        fixture.notebooks.append({"normal", "操作验证", "normal", {}, {}});
        fixture.notebooks.append({"ebook", "章节验证", "ebook", {}, {}});
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
        QCOMPARE(window.findChild<QPlainTextEdit *>("editor")->toPlainText(), QString());
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

    void exportsContainUserContent() {
        NotebookStore fixture;
        fixture.notebooks.append({"site", "导出站点", "site", {{"post", {}, "文章", "# 用户文章\n\n正文", false, {}}}, {{"title", "用户站点"}}});
        fixture.notebooks.append({"ebook", "导出书籍", "ebook", {{"chapter", {}, "章节", "# 用户章节\n\n章节正文", false, {}}}, {}});
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
        Notebook site{"site", "测试站点", "site", {}, {{"title", "用户的站点"}}};
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
        QVERIFY(window.findChild<QPlainTextEdit *>("editor")->height() > 100);
        QTest::qWait(30);
        window.grab().save("site-narrow.png");
        window.resize(360, 640);
        QTest::qWait(30);
        QCOMPARE(window.size(), QSize(360, 640));
        QVERIFY(window.findChild<QPlainTextEdit *>("editor")->height() > 100);
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
QTEST_MAIN(UiTest)
#include "UiTest.moc"
