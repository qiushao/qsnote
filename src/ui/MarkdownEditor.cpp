#include "MarkdownEditor.h"
#include <QActionGroup>
#include <QBuffer>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QFontComboBox>
#include <QImageReader>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QScopedValueRollback>
#include <QSettings>
#include <QShortcut>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>
#include <functional>
#include <vtextedit/markdowneditorconfig.h>
#include <vtextedit/markdownutils.h>
#include <vtextedit/viconfig.h>
#include <vtextedit/vtextedit.h>

namespace {
QSharedPointer<vte::MarkdownEditorConfig> editorConfig() {
    vte::VTextEditor::addSyntaxCustomSearchPaths({":/demo/data"});
    QSettings settings;
    settings.beginGroup("editor");
    auto text = QSharedPointer<vte::TextEditorConfig>::create();
    text->m_viConfig = QSharedPointer<vte::ViConfig>::create();
    text->m_inputMode = static_cast<vte::InputMode>(settings.value("inputMode", vte::NormalMode).toInt());
    text->m_lineNumberType = static_cast<vte::VTextEditor::LineNumberType>(settings.value("lineNumbers", 1).toInt());
    text->m_wrapMode = static_cast<vte::WrapMode>(settings.value("wrapMode", vte::WordWrapOrAnywhere).toInt());
    text->m_centerCursor = static_cast<vte::CenterCursor>(settings.value("centerCursor", vte::NeverCenter).toInt());
    text->m_textFoldingEnabled = settings.value("folding", true).toBool();
    text->m_expandTab = settings.value("expandTab", true).toBool();
    text->m_tabStopWidth = settings.value("tabWidth", 4).toInt();
    text->m_highlightWhitespace = settings.value("whitespace", false).toBool();
    text->m_lineSpacing = settings.value("lineSpacing", 1.0).toDouble();
    text->m_maxContentWidth = settings.value("contentWidth", 0).toInt();
    // The library default is shared. Keep font overrides local to this config.
    text->m_theme = QSharedPointer<vte::Theme>::create(*vte::TextEditorConfig::defaultTheme());
    auto config = QSharedPointer<vte::MarkdownEditorConfig>::create(text);
    config->overrideTextFontFamily(settings.value("fontFamily").toString());
    auto &font = text->m_theme->editorStyle(vte::Theme::Text);
    font.m_fontPointSize = settings.value("fontSize", font.m_fontPointSize).toInt();
    config->m_webCodeBlockHighlighterEnabled = false;
    config->m_inplacePreviewSources |= vte::MarkdownEditorConfig::Table;
    config->m_inplacePreviewSources.setFlag(vte::MarkdownEditorConfig::ImageLink, settings.value("previewImages", true).toBool());
    config->m_inplacePreviewSources.setFlag(vte::MarkdownEditorConfig::Table, settings.value("previewTables", true).toBool());
    config->m_constrainInplacePreviewWidthEnabled = settings.value("constrainPreviewWidth", false).toBool();
    config->m_autoFoldPreviewedBlocksEnabled = settings.value("autoFoldPreviews", true).toBool();
    if (!settings.value("concealLinks", true).toBool()) config->m_concealElements = {};
    config->m_autoFormatTableSourceEnabled = settings.value("formatTables", true).toBool();
    config->m_autoNumberOrderedListsEnabled = settings.value("numberLists", true).toBool();
    return config;
}
}// namespace

MarkdownEditor::MarkdownEditor(QWidget *parent)
    : MarkdownEditor(editorConfig(), parent) {}

MarkdownEditor::MarkdownEditor(const QSharedPointer<vte::MarkdownEditorConfig> &config, QWidget *parent)
    : VMarkdownEditor(config, QSharedPointer<vte::TextEditorParameters>::create(), parent), config_(config) {
    setObjectName("markdownEditor");
    getTextEdit()->setObjectName("editor");
    enableInternalContextMenu();
    reloadSettings();
    connect(this, &VMarkdownEditor::spellCheckStateChanged, this, [this] {
        if (applyingSettings_) return;
        QSettings settings;
        settings.setValue("editor/spellCheck", isSpellCheckEnabled());
        settings.setValue("editor/autoDetectLanguage", isAutoDetectLanguageEnabled());
        settings.setValue("editor/spellLanguage", currentSpellCheckLanguage());
        emit settingsChanged();
    });
    connect(this, &VMarkdownEditor::imageInsertionRequested, this, &MarkdownEditor::insertImage);
    const QList<QPair<QKeySequence, vte::TypeAction>> shortcuts = {
            {QKeySequence::Bold, vte::TypeAction::TypeBold},
            {QKeySequence::Italic, vte::TypeAction::TypeItalic},
            {QKeySequence("Ctrl+Shift+S"), vte::TypeAction::TypeStrikethrough},
            {QKeySequence("Ctrl+K"), vte::TypeAction::TypeLink}};
    for (const auto &entry: shortcuts) {
        auto *shortcut = new QShortcut(entry.first, this);
        shortcut->setContext(Qt::WidgetWithChildrenShortcut);
        connect(shortcut, &QShortcut::activated, this, [this, action = entry.second] { type(action); });
    }
}

void MarkdownEditor::type(vte::TypeAction action, const QVariant &data) {
    if (handleTypeAction(action, data)) return;
    if (action == vte::TypeAction::TypeImage) {
        insertImage(0, getTextEdit()->textCursor().selectedText());
    } else if (action == vte::TypeAction::TypeLink) {
        QDialog dialog(this);
        dialog.setWindowTitle("插入链接");
        auto *layout = new QFormLayout(&dialog);
        auto *text = new QLineEdit(getTextEdit()->textCursor().selectedText(), &dialog);
        auto *url = new QLineEdit(&dialog);
        text->setObjectName("linkText");
        url->setObjectName("linkUrl");
        layout->addRow("文字", text);
        layout->addRow("地址", url);
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        layout->addRow(buttons);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        if (dialog.exec() == QDialog::Accepted && !url->text().isEmpty()) {
            handleTypeAction(action, QStringList{text->text(), url->text()});
        }
    } else if (action == vte::TypeAction::TypeTable) {
        QDialog dialog(this);
        dialog.setWindowTitle("插入表格");
        auto *layout = new QFormLayout(&dialog);
        auto *rows = new QSpinBox(&dialog);
        auto *columns = new QSpinBox(&dialog);
        rows->setRange(1, 100);
        columns->setRange(1, 50);
        rows->setValue(3);
        columns->setValue(3);
        layout->addRow("数据行数", rows);
        layout->addRow("列数", columns);
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        layout->addRow(buttons);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        if (dialog.exec() != QDialog::Accepted) return;
        QString table = "\n|";
        for (int col = 0; col < columns->value(); ++col) table += QStringLiteral(" 列%1 |").arg(col + 1);
        table += "\n|";
        for (int col = 0; col < columns->value(); ++col) table += " --- |";
        for (int row = 0; row < rows->value(); ++row) {
            table += "\n|";
            for (int col = 0; col < columns->value(); ++col) table += "  |";
        }
        insertText(table + "\n");
    }
}

void MarkdownEditor::insertImage(quint64 requestId, const QString &selectedText) {
    const auto path = QFileDialog::getOpenFileName(this, "插入图片", {}, "图片 (*.png *.jpg *.jpeg *.gif *.webp *.bmp *.svg)");
    if (path.isEmpty()) {
        if (requestId) cancelImageInsertion(requestId);
        return;
    }
    const auto image = QImageReader(path).read();
    if (image.isNull()) {
        if (requestId) cancelImageInsertion(requestId);
        QMessageBox::warning(this, "无法插入图片", "无法读取所选图片。");
        return;
    }
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    const auto link = vte::MarkdownUtils::generateImageLink(selectedText, "data:image/png;base64," + QString::fromLatin1(bytes.toBase64()), {});
    if (requestId) completeImageInsertion(requestId, link);
    else
        insertText(link);
}

void MarkdownEditor::showFindReplace() {
    QDialog dialog(this);
    dialog.setWindowTitle("查找和替换");
    dialog.setObjectName("findReplace");
    auto *layout = new QVBoxLayout(&dialog);
    auto *fields = new QFormLayout;
    auto *query = new QLineEdit(getTextEdit()->textCursor().selectedText(), &dialog);
    auto *replacement = new QLineEdit(&dialog);
    query->setObjectName("findText");
    replacement->setObjectName("replaceText");
    fields->addRow("查找", query);
    fields->addRow("替换为", replacement);
    layout->addLayout(fields);
    auto *options = new QHBoxLayout;
    auto *sensitive = new QCheckBox("区分大小写", &dialog);
    auto *whole = new QCheckBox("全词", &dialog);
    auto *regex = new QCheckBox("正则表达式", &dialog);
    for (auto *check: {sensitive, whole, regex}) options->addWidget(check);
    layout->addLayout(options);
    auto *result = new QLabel(&dialog);
    result->setObjectName("findResult");
    layout->addWidget(result);
    auto *actions = new QHBoxLayout;
    const QStringList labels = {"上一个", "下一个", "替换", "全部替换"};
    for (int index = 0; index < labels.size(); ++index) {
        auto *button = new QPushButton(labels[index], &dialog);
        button->setObjectName(QStringLiteral("findAction%1").arg(index));
        actions->addWidget(button);
        connect(button, &QPushButton::clicked, &dialog, [=] {
            if (query->text().isEmpty()) return;
            if (regex->isChecked() && !QRegularExpression(query->text()).isValid()) {
                result->setText("正则表达式无效");
                return;
            }
            vte::FindFlags flags;
            if (sensitive->isChecked()) flags |= vte::CaseSensitive;
            if (whole->isChecked()) flags |= vte::WholeWordOnly;
            if (regex->isChecked()) flags |= vte::RegularExpression;
            if (index == 0) flags |= vte::FindBackward;
            const auto found = index < 2    ? findText({query->text()}, flags)
                               : index == 2 ? replaceText(query->text(), flags, replacement->text())
                                            : replaceAll(query->text(), flags, replacement->text());
            result->setText(QStringLiteral("%1 个匹配%2").arg(found.m_totalMatches).arg(found.m_wrapped ? "（已回到开头）" : ""));
        });
    }
    layout->addLayout(actions);
    query->setFocus();
    dialog.exec();
    clearSearchHighlight();
    getTextEdit()->setFocus();
}

void MarkdownEditor::reloadSettings() {
    const QScopedValueRollback<bool> guard(applyingSettings_, true);
    zoom(0);
    config_ = editorConfig();
    setConfig(config_);
    QSettings settings;
    setInplacePreviewEnabled(settings.value("editor/inplacePreview", true).toBool());
    setSpellCheckLanguage(settings.value("editor/spellLanguage", "en_US").toString());
    setAutoDetectLanguageEnabled(settings.value("editor/autoDetectLanguage", false).toBool());
    setSpellCheckEnabled(settings.value("editor/spellCheck", false).toBool());
    zoom(settings.value("editor/zoom", 0).toInt());
}

void MarkdownEditor::saveSetting(const QString &key, const QVariant &value) {
    QSettings settings;
    settings.setValue("editor/" + key, value);
    reloadSettings();
    emit settingsChanged();
}

void MarkdownEditor::showSettings() {
    QDialog dialog(this);
    dialog.setObjectName("editorSettings");
    dialog.setWindowTitle("编辑器设置（所有笔记）");
    auto *layout = new QVBoxLayout(&dialog);
    auto *tabs = new QTabWidget(&dialog);
    layout->addWidget(tabs);
    auto page = [&tabs](const QString &title) {
        auto *widget = new QWidget(tabs);
        tabs->addTab(widget, title);
        return new QFormLayout(widget);
    };
    auto *general = page("文本");
    auto *markdown = page("Markdown");
    auto *spelling = page("拼写");
    QSettings settings;
    settings.beginGroup("editor");
    // Read controls only after acceptance, so Cancel never changes live settings.
    QList<std::function<void()>> saveControls;
    auto combo = [&](QFormLayout *form, const QString &key, const QString &label, const QStringList &items, int current) {
        auto *input = new QComboBox(&dialog);
        input->setObjectName(key);
        input->addItems(items);
        input->setCurrentIndex(current);
        form->addRow(label, input);
        saveControls.append([&, key, input] { settings.setValue(key, input->currentIndex()); });
    };
    auto check = [&](QFormLayout *form, const QString &key, const QString &label, bool current) {
        auto *input = new QCheckBox(label, &dialog);
        input->setObjectName(key);
        input->setChecked(current);
        form->addRow(input);
        saveControls.append([&, key, input] { settings.setValue(key, input->isChecked()); });
    };
    auto number = [&](QFormLayout *form, const QString &key, const QString &label, int current, int minimum, int maximum) {
        auto *input = new QSpinBox(&dialog);
        input->setObjectName(key);
        input->setRange(minimum, maximum);
        input->setValue(current);
        form->addRow(label, input);
        saveControls.append([&, key, input] { settings.setValue(key, input->value()); });
    };
    const auto &config = getConfig();
    combo(general, "inputMode", "输入模式", {"普通", "Vim", "VS Code"}, config.m_inputMode);
    combo(general, "lineNumbers", "行号", {"隐藏", "绝对行号", "相对行号"}, static_cast<int>(config.m_lineNumberType));
    combo(general, "wrapMode", "换行", {"不换行", "按单词", "任意位置", "优先按单词"}, config.m_wrapMode);
    combo(general, "centerCursor", "光标居中", {"不居中", "始终居中", "到达底部时居中"}, config.m_centerCursor);
    auto *font = new QFontComboBox(&dialog);
    font->setObjectName("fontFamily");
    font->setCurrentFont(getTextEdit()->font());
    general->addRow("字体", font);
    saveControls.append([&, font] { settings.setValue("fontFamily", font->currentFont().family()); });
    number(general, "fontSize", "字号（pt）", baseEditorFontPointSize(), 6, 72);
    number(general, "zoom", "字号缩放增量", zoomDelta(), -5, 50);
    number(general, "tabWidth", "Tab 宽度", config.m_tabStopWidth, 1, 32);
    auto *spacing = new QDoubleSpinBox(&dialog);
    spacing->setObjectName("lineSpacing");
    spacing->setRange(1.0, 5.0);
    spacing->setSingleStep(0.1);
    spacing->setDecimals(1);
    spacing->setValue(config.m_lineSpacing);
    general->addRow("行距倍数", spacing);
    saveControls.append([&, spacing] { settings.setValue("lineSpacing", spacing->value()); });
    number(general, "contentWidth", "最大正文宽度（px，0 为不限）", config.m_maxContentWidth, 0, 3000);
    check(general, "expandTab", "将 Tab 展开为空格", config.m_expandTab);
    check(general, "folding", "允许文本折叠", config.m_textFoldingEnabled);
    check(general, "whitespace", "标记 Tab 和行尾空白", config.m_highlightWhitespace);
    check(markdown, "inplacePreview", "启用就地预览", settings.value("inplacePreview", true).toBool());
    check(markdown, "previewImages", "图片就地预览", config_->m_inplacePreviewSources.testFlag(vte::MarkdownEditorConfig::ImageLink));
    check(markdown, "previewTables", "可编辑表格就地预览", config_->m_inplacePreviewSources.testFlag(vte::MarkdownEditorConfig::Table));
    check(markdown, "constrainPreviewWidth", "限制就地预览宽度", config_->m_constrainInplacePreviewWidthEnabled);
    check(markdown, "autoFoldPreviews", "自动折叠已有预览的源码", config_->m_autoFoldPreviewedBlocksEnabled);
    check(markdown, "concealLinks", "缩短显示过长的链接地址", bool(config_->m_concealElements));
    check(markdown, "formatTables", "自动对齐表格源码", config_->m_autoFormatTableSourceEnabled);
    check(markdown, "numberLists", "自动编号有序列表", config_->m_autoNumberOrderedListsEnabled);
    check(spelling, "spellCheck", "启用拼写检查", isSpellCheckEnabled());
    check(spelling, "autoDetectLanguage", "自动识别语言", isAutoDetectLanguageEnabled());
    auto *languages = new QComboBox(&dialog);
    languages->setObjectName("spellLanguage");
    const auto dictionaries = availableSpellCheckDictionaries();
    for (auto it = dictionaries.begin(); it != dictionaries.end(); ++it) languages->addItem(it.key(), it.value());
    int selected = languages->findData(currentSpellCheckLanguage());
    if (selected < 0) {
        languages->addItem(currentSpellCheckLanguage() + "（未安装）", currentSpellCheckLanguage());
        selected = languages->count() - 1;
    }
    languages->setCurrentIndex(selected);
    spelling->addRow("词典", languages);
    if (dictionaries.isEmpty()) spelling->addRow(new QLabel("未找到拼写词典，请安装所需语言的 Hunspell 词典。", &dialog));
    saveControls.append([&, languages] { settings.setValue("spellLanguage", languages->currentData()); });
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return;
    for (const auto &save: saveControls) save();
    settings.sync();
    reloadSettings();
    emit settingsChanged();
}

QMenu *MarkdownEditor::createEditorMenu(QWidget *parent) {
    auto *menu = new QMenu(parent);
    menu->addAction("查找和替换…", this, &MarkdownEditor::showFindReplace);
    auto *headings = menu->addMenu("标题级别");
    for (int level = 0; level <= 6; ++level) {
        headings->addAction(level == 0 ? QString("正文") : QStringLiteral("H%1").arg(level), this,
                            [this, level] { type(vte::TypeAction::TypeHeading, level); });
    }
    menu->addAction("已完成任务", this, [this] { type(vte::TypeAction::TypeTodoList, true); });
    menu->addAction("折叠当前区域", this, [this] { foldAtCursor(); });
    menu->addAction("展开当前区域", this, [this] { unfoldAtCursor(); });
    menu->addAction("补全", this, [this] { getTextEdit()->setFocus(); triggerCompletion(false); });
    menu->addAction("放大", this, [this] { saveSetting("zoom", zoomDelta() + 1); });
    menu->addAction("缩小", this, [this] { saveSetting("zoom", zoomDelta() - 1); });
    auto *modes = menu->addMenu("输入模式");
    auto *group = new QActionGroup(modes);
    const QStringList names = {"普通", "Vim", "VS Code"};
    for (int index = 0; index < names.size(); ++index) {
        auto *action = modes->addAction(names[index]);
        action->setCheckable(true);
        action->setChecked(index == getConfig().m_inputMode);
        group->addAction(action);
        connect(menu, &QMenu::aboutToShow, action, [this, action, index] { action->setChecked(index == getConfig().m_inputMode); });
        connect(action, &QAction::triggered, this, [this, index] {
            saveSetting("inputMode", index);
            getTextEdit()->setFocus();
        });
    }
    menu->addSeparator();
    auto toggle = [this, menu](const QString &label, const QString &key, const QVariant &on, const QVariant &off, const QVariant &fallback) {
        auto *action = menu->addAction(label);
        action->setCheckable(true);
        auto update = [action, key, off, fallback] { action->setChecked(QSettings().value("editor/" + key, fallback) != off); };
        update();
        connect(menu, &QMenu::aboutToShow, action, update);
        connect(action, &QAction::triggered, this, [this, key, on, off](bool enabled) { saveSetting(key, enabled ? on : off); });
    };
    toggle("显示行号", "lineNumbers", 1, 0, 1);
    toggle("自动换行", "wrapMode", int(vte::WordWrapOrAnywhere), int(vte::NoWrap), int(vte::WordWrapOrAnywhere));
    toggle("图片和表格就地预览", "inplacePreview", true, false, true);
    toggle("自动对齐表格源码", "formatTables", true, false, true);
    toggle("自动编号有序列表", "numberLists", true, false, true);
    toggle("光标居中", "centerCursor", int(vte::AlwaysCenter), int(vte::NeverCenter), int(vte::NeverCenter));
    toggle("拼写检查", "spellCheck", true, false, false);
    menu->addSeparator();
    menu->addAction("编辑器设置…", this, &MarkdownEditor::showSettings);
    return menu;
}
