#include "MarkdownEditor.h"
#include "InplacePreview.h"
#include <QActionGroup>
#include <QBuffer>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFontComboBox>
#include <QImageReader>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QScopedValueRollback>
#include <QSettings>
#include <QShortcut>
#include <QSpinBox>
#include <QTabWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <functional>
#include <vtextedit/markdowneditorconfig.h>
#include <vtextedit/markdownutils.h>
#include <vtextedit/viconfig.h>
#include <vtextedit/vtextedit.h>

namespace {
QStringList imagePaths(const QMimeData *source) {
    QStringList paths;
    for (const auto &url: source->urls()) {
        if (!url.isLocalFile() || !QImageReader(url.toLocalFile()).canRead()) return {};
        paths.append(url.toLocalFile());
    }
    return paths;
}

QString imageLink(const QImage &image, const QString &description) {
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return vte::MarkdownUtils::generateImageLink(description, "data:image/png;base64," + QString::fromLatin1(bytes.toBase64()), {});
}

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
    font.m_backgroundColor = qRgb(255, 255, 255);
    text->m_theme->editorStyle(vte::Theme::CursorLine).m_backgroundColor = qRgb(244, 247, 252);
    text->m_theme->editorStyle(vte::Theme::ContentMargin).m_backgroundColor = font.m_backgroundColor;
    text->m_theme->editorStyle(vte::Theme::IndicatorsBorder).m_backgroundColor = font.m_backgroundColor;
    config->m_webCodeBlockHighlighterEnabled = true;
    config->m_inplacePreviewSources |= vte::MarkdownEditorConfig::Table;
    config->m_inplacePreviewSources.setFlag(vte::MarkdownEditorConfig::ImageLink, settings.value("previewImages", true).toBool());
    config->m_inplacePreviewSources.setFlag(vte::MarkdownEditorConfig::Table, settings.value("previewTables", true).toBool());
    config->m_inplacePreviewSources.setFlag(vte::MarkdownEditorConfig::Math, settings.value("previewMath", true).toBool());
    config->m_inplacePreviewSources.setFlag(vte::MarkdownEditorConfig::CodeBlock, settings.value("previewGraphs", true).toBool() || settings.value("previewMath", true).toBool());
    config->m_constrainInplacePreviewWidthEnabled = settings.value("constrainPreviewWidth", false).toBool();
    config->m_autoFoldPreviewedBlocksEnabled = settings.value("autoFoldPreviews", false).toBool();
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
    getTextEdit()->viewport()->installEventFilter(this);
    enableInternalContextMenu();
    inplacePreview_ = new InplacePreview(this);
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
    connect(getTextEdit(), &vte::VTextEdit::canInsertFromMimeDataRequested, this,
            [this](const QMimeData *source, bool *handled, bool *allowed) {
        if (!source->hasImage() && imagePaths(source).isEmpty()) return;
        *handled = true;
        *allowed = !getTextEdit()->isReadOnly()
            && (!source->hasImage() || !qvariant_cast<QImage>(source->imageData()).isNull());
    });
    connect(getTextEdit(), &vte::VTextEdit::insertFromMimeDataRequested, this,
            [this](const QMimeData *source, bool *handled) {
        const auto paths = imagePaths(source);
        if (!source->hasImage() && paths.isEmpty()) return;
        *handled = true;
        if (getTextEdit()->isReadOnly()) return;
        if (source->hasImage()) {
            const auto image = qvariant_cast<QImage>(source->imageData());
            if (!image.isNull()) insertText(imageLink(image, getTextEdit()->textCursor().selectedText()));
            return;
        }
        QStringList links;
        for (const auto &path: paths) {
            const auto image = QImageReader(path).read();
            if (image.isNull()) {
                QMessageBox::warning(this, tr("无法插入图片"), tr("无法读取所选图片。"));
                return;
            }
            links.append(imageLink(image, QFileInfo(path).completeBaseName()));
        }
        insertText(links.join('\n'));
    });
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
        dialog.setWindowTitle(tr("插入链接"));
        auto *layout = new QFormLayout(&dialog);
        auto *text = new QLineEdit(getTextEdit()->textCursor().selectedText(), &dialog);
        auto *url = new QLineEdit(&dialog);
        text->setObjectName("linkText");
        url->setObjectName("linkUrl");
        layout->addRow(tr("文字"), text);
        layout->addRow(tr("地址"), url);
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        layout->addRow(buttons);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        if (dialog.exec() == QDialog::Accepted && !url->text().isEmpty()) {
            handleTypeAction(action, QStringList{text->text(), url->text()});
        }
    } else if (action == vte::TypeAction::TypeTable) {
        QDialog dialog(this);
        dialog.setWindowTitle(tr("插入表格"));
        auto *layout = new QFormLayout(&dialog);
        auto *rows = new QSpinBox(&dialog);
        auto *columns = new QSpinBox(&dialog);
        rows->setRange(1, 100);
        columns->setRange(1, 50);
        rows->setValue(3);
        columns->setValue(3);
        layout->addRow(tr("数据行数"), rows);
        layout->addRow(tr("列数"), columns);
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        layout->addRow(buttons);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        if (dialog.exec() != QDialog::Accepted) return;
        QString table = "\n|";
        for (int col = 0; col < columns->value(); ++col) table += tr(" 列%1 |").arg(col + 1);
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
    const auto path = QFileDialog::getOpenFileName(this, tr("插入图片"), {}, tr("图片 (*.png *.jpg *.jpeg *.gif *.webp *.bmp *.svg)"));
    if (path.isEmpty()) {
        if (requestId) cancelImageInsertion(requestId);
        return;
    }
    const auto image = QImageReader(path).read();
    if (image.isNull()) {
        if (requestId) cancelImageInsertion(requestId);
        QMessageBox::warning(this, tr("无法插入图片"), tr("无法读取所选图片。"));
        return;
    }
    const auto link = imageLink(image, selectedText);
    if (requestId) completeImageInsertion(requestId, link);
    else
        insertText(link);
}

void MarkdownEditor::showFindReplace() {
    QDialog dialog(this);
    dialog.setWindowTitle(tr("查找和替换"));
    dialog.setObjectName("findReplace");
    auto *layout = new QVBoxLayout(&dialog);
    auto *fields = new QFormLayout;
    auto *query = new QLineEdit(getTextEdit()->textCursor().selectedText(), &dialog);
    auto *replacement = new QLineEdit(&dialog);
    query->setObjectName("findText");
    replacement->setObjectName("replaceText");
    fields->addRow(tr("查找"), query);
    fields->addRow(tr("替换为"), replacement);
    layout->addLayout(fields);
    auto *options = new QHBoxLayout;
    auto *sensitive = new QCheckBox(tr("区分大小写"), &dialog);
    auto *whole = new QCheckBox(tr("全词"), &dialog);
    auto *regex = new QCheckBox(tr("正则表达式"), &dialog);
    for (auto *check: {sensitive, whole, regex}) options->addWidget(check);
    layout->addLayout(options);
    auto *result = new QLabel(&dialog);
    result->setObjectName("findResult");
    layout->addWidget(result);
    auto *actions = new QHBoxLayout;
    const QStringList labels = {tr("上一个"), tr("下一个"), tr("替换"), tr("全部替换")};
    for (int index = 0; index < labels.size(); ++index) {
        auto *button = new QPushButton(labels[index], &dialog);
        button->setObjectName(QStringLiteral("findAction%1").arg(index));
        actions->addWidget(button);
        connect(button, &QPushButton::clicked, &dialog, [=] {
            if (query->text().isEmpty()) return;
            if (regex->isChecked() && !QRegularExpression(query->text()).isValid()) {
                result->setText(tr("正则表达式无效"));
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
            result->setText(tr("%1 个匹配%2").arg(found.m_totalMatches).arg(found.m_wrapped ? tr("（已回到开头）") : ""));
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

void MarkdownEditor::zoom(int delta) {
    VMarkdownEditor::zoom(delta);
    if (inplacePreview_) inplacePreview_->refresh();
}

bool MarkdownEditor::eventFilter(QObject *watched, QEvent *event) {
    if (watched == getTextEdit()->viewport() && event->type() == QEvent::Wheel) {
        auto *wheel = dynamic_cast<QWheelEvent *>(event);
        if (wheel && (wheel->modifiers() & Qt::ControlModifier)) {
            const int steps = wheel->angleDelta().y() / 120;
            const int delta = qMax(2 - baseEditorFontPointSize(), zoomDelta() + steps);
            if (steps != 0 && delta != zoomDelta()) {
                zoom(delta);
                QSettings settings;
                settings.setValue("editor/zoom", zoomDelta());
                settings.sync();
                emit settingsChanged();
            }
            wheel->accept();
            return true;
        }
    }
    return VMarkdownEditor::eventFilter(watched, event);
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
    dialog.setWindowTitle(tr("编辑器设置（所有笔记）"));
    auto *layout = new QVBoxLayout(&dialog);
    std::function<void()> apply;
    layout->addWidget(createSettingsPage(&dialog, apply));
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() == QDialog::Accepted) apply();
}

QWidget *MarkdownEditor::createSettingsPage(QWidget *parent, std::function<void()> &apply) {
    auto *tabs = new QTabWidget(parent);
    auto page = [&tabs](const QString &title) {
        auto *widget = new QWidget(tabs);
        tabs->addTab(widget, title);
        auto *form = new QFormLayout(widget);
        form->setRowWrapPolicy(QFormLayout::WrapLongRows);
        form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        return form;
    };
    auto *general = page(tr("文本"));
    auto *markdown = page("Markdown");
    auto *spelling = page(tr("拼写"));
    QSettings settings;
    settings.beginGroup("editor");
    // Read controls only after acceptance, so Cancel never changes live settings.
    QList<std::function<void()>> saveControls;
    auto combo = [&](QFormLayout *form, const QString &key, const QString &label, const QStringList &items, int current) {
        auto *input = new QComboBox(tabs);
        input->setObjectName(key);
        input->addItems(items);
        input->setCurrentIndex(current);
        form->addRow(label, input);
        saveControls.append([key, input] { QSettings().setValue("editor/" + key, input->currentIndex()); });
    };
    auto check = [&](QFormLayout *form, const QString &key, const QString &label, bool current) {
        auto *input = new QCheckBox(label, tabs);
        input->setObjectName(key);
        input->setChecked(current);
        form->addRow(input);
        saveControls.append([key, input] { QSettings().setValue("editor/" + key, input->isChecked()); });
    };
    auto number = [&](QFormLayout *form, const QString &key, const QString &label, int current, int minimum, int maximum) {
        auto *input = new QSpinBox(tabs);
        input->setObjectName(key);
        input->setRange(minimum, maximum);
        input->setValue(current);
        form->addRow(label, input);
        saveControls.append([key, input] { QSettings().setValue("editor/" + key, input->value()); });
    };
    const auto &config = getConfig();
    combo(general, "inputMode", tr("输入模式"), {tr("普通"), "Vim", "VS Code"}, config.m_inputMode);
    combo(general, "lineNumbers", tr("行号"), {tr("隐藏"), tr("绝对行号"), tr("相对行号")}, static_cast<int>(config.m_lineNumberType));
    combo(general, "wrapMode", tr("换行"), {tr("不换行"), tr("按单词"), tr("任意位置"), tr("优先按单词")}, config.m_wrapMode);
    combo(general, "centerCursor", tr("光标居中"), {tr("不居中"), tr("始终居中"), tr("到达底部时居中")}, config.m_centerCursor);
    auto *font = new QFontComboBox(tabs);
    font->setObjectName("fontFamily");
    font->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    font->setCurrentFont(getTextEdit()->font());
    general->addRow(tr("字体"), font);
    saveControls.append([font] { QSettings().setValue("editor/fontFamily", font->currentFont().family()); });
    number(general, "fontSize", tr("字号（pt）"), baseEditorFontPointSize(), 6, 72);
    number(general, "zoom", tr("字号缩放增量"), zoomDelta(), -5, 50);
    number(general, "tabWidth", tr("Tab 宽度"), config.m_tabStopWidth, 1, 32);
    auto *spacing = new QDoubleSpinBox(tabs);
    spacing->setObjectName("lineSpacing");
    spacing->setRange(1.0, 5.0);
    spacing->setSingleStep(0.1);
    spacing->setDecimals(1);
    spacing->setValue(config.m_lineSpacing);
    general->addRow(tr("行距倍数"), spacing);
    saveControls.append([spacing] { QSettings().setValue("editor/lineSpacing", spacing->value()); });
    number(general, "contentWidth", tr("最大正文宽度（px，0 为不限）"), config.m_maxContentWidth, 0, 3000);
    check(general, "expandTab", tr("将 Tab 展开为空格"), config.m_expandTab);
    check(general, "folding", tr("允许文本折叠"), config.m_textFoldingEnabled);
    check(general, "whitespace", tr("标记 Tab 和行尾空白"), config.m_highlightWhitespace);
    check(markdown, "inplacePreview", tr("启用就地预览"), settings.value("inplacePreview", true).toBool());
    check(markdown, "previewImages", tr("图片就地预览"), config_->m_inplacePreviewSources.testFlag(vte::MarkdownEditorConfig::ImageLink));
    check(markdown, "previewTables", tr("可编辑表格就地预览"), config_->m_inplacePreviewSources.testFlag(vte::MarkdownEditorConfig::Table));
    check(markdown, "previewMath", tr("公式就地预览"), config_->m_inplacePreviewSources.testFlag(vte::MarkdownEditorConfig::Math));
    check(markdown, "previewGraphs", tr("图表就地预览"), settings.value("previewGraphs", true).toBool());
    check(markdown, "constrainPreviewWidth", tr("限制就地预览宽度"), config_->m_constrainInplacePreviewWidthEnabled);
    check(markdown, "autoFoldPreviews", tr("自动折叠已有预览的源码"), config_->m_autoFoldPreviewedBlocksEnabled);
    check(markdown, "concealLinks", tr("缩短显示过长的链接地址"), bool(config_->m_concealElements));
    check(markdown, "formatTables", tr("自动对齐表格源码"), config_->m_autoFormatTableSourceEnabled);
    check(markdown, "numberLists", tr("自动编号有序列表"), config_->m_autoNumberOrderedListsEnabled);
    check(spelling, "spellCheck", tr("启用拼写检查"), isSpellCheckEnabled());
    check(spelling, "autoDetectLanguage", tr("自动识别语言"), isAutoDetectLanguageEnabled());
    auto *languages = new QComboBox(tabs);
    languages->setObjectName("spellLanguage");
    const auto dictionaries = availableSpellCheckDictionaries();
    for (auto it = dictionaries.begin(); it != dictionaries.end(); ++it) languages->addItem(it.key(), it.value());
    int selected = languages->findData(currentSpellCheckLanguage());
    if (selected < 0) {
        languages->addItem(currentSpellCheckLanguage() + tr("（未安装）"), currentSpellCheckLanguage());
        selected = languages->count() - 1;
    }
    languages->setCurrentIndex(selected);
    spelling->addRow(tr("词典"), languages);
    if (dictionaries.isEmpty()) {
        auto *hint = new QLabel(tr("未找到拼写词典，请安装所需语言的 Hunspell 词典。"), tabs);
        hint->setWordWrap(true);
        spelling->addRow(hint);
    }
    saveControls.append([languages] { QSettings().setValue("editor/spellLanguage", languages->currentData()); });
    apply = [this, saveControls] {
        for (const auto &save: saveControls) save();
        QSettings().sync();
        reloadSettings();
        emit settingsChanged();
    };
    for (auto *label: tabs->findChildren<QLabel *>()) label->setWordWrap(true);
    tabs->ensurePolished();
    tabs->setMinimumHeight(tabs->sizeHint().height());
    return tabs;
}

QMenu *MarkdownEditor::createEditorMenu(QWidget *parent) {
    auto *menu = new QMenu(parent);
    menu->addAction(tr("查找和替换…"), this, &MarkdownEditor::showFindReplace);
    auto *headings = menu->addMenu(tr("标题级别"));
    for (int level = 0; level <= 6; ++level) {
        headings->addAction(level == 0 ? tr("正文") : QStringLiteral("H%1").arg(level), this,
                            [this, level] { type(vte::TypeAction::TypeHeading, level); });
    }
    menu->addAction(tr("已完成任务"), this, [this] { type(vte::TypeAction::TypeTodoList, true); });
    menu->addAction(tr("折叠当前区域"), this, [this] { foldAtCursor(); });
    menu->addAction(tr("展开当前区域"), this, [this] { unfoldAtCursor(); });
    menu->addAction(tr("补全"), this, [this] { getTextEdit()->setFocus(); triggerCompletion(false); });
    menu->addAction(tr("放大"), this, [this] { saveSetting("zoom", zoomDelta() + 1); });
    menu->addAction(tr("缩小"), this, [this] { saveSetting("zoom", zoomDelta() - 1); });
    auto *modes = menu->addMenu(tr("输入模式"));
    auto *group = new QActionGroup(modes);
    const QStringList names = {tr("普通"), "Vim", "VS Code"};
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
    toggle(tr("显示行号"), "lineNumbers", 1, 0, 1);
    toggle(tr("自动换行"), "wrapMode", int(vte::WordWrapOrAnywhere), int(vte::NoWrap), int(vte::WordWrapOrAnywhere));
    toggle(tr("图片和表格就地预览"), "inplacePreview", true, false, true);
    toggle(tr("自动对齐表格源码"), "formatTables", true, false, true);
    toggle(tr("自动编号有序列表"), "numberLists", true, false, true);
    toggle(tr("光标居中"), "centerCursor", int(vte::AlwaysCenter), int(vte::NeverCenter), int(vte::NeverCenter));
    toggle(tr("拼写检查"), "spellCheck", true, false, false);
    menu->addSeparator();
    menu->addAction(tr("编辑器设置…"), this, &MarkdownEditor::showSettings);
    return menu;
}
