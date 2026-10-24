#include "SettingsDialog.h"
#include "MarkdownEditor.h"
#include "UiStyle.h"
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QFontComboBox>
#include <QFormLayout>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QSpinBox>
#include <QTabBar>

SettingsDialog::SettingsDialog(MarkdownEditor *editor, QWidget *parent) : OverlayDialog(parent, "settingsDialog", true) {
    auto *layout = column(this);
    auto *bar = new QWidget(this);
    bar->setObjectName("dialogBar");
    auto *barLayout = row(bar, 16, 11);
    auto *title = label(QCoreApplication::translate("MainWindow", "设置"), bar);
    title->setProperty("dialogTitle", true);
    barLayout->addWidget(title);
    barLayout->addStretch();
    auto *close = button(QCoreApplication::translate("MainWindow", "关闭"), "closeSettings", bar);
    barLayout->addWidget(close);
    layout->addWidget(bar);
    auto *navigation = new QWidget(this);
    navigation->setObjectName("settingsNavigation");
    auto *navigationLayout = row(navigation, 16, 10);
    auto *sections = new QTabBar(navigation);
    sections->setObjectName("settingsTabs");
    sections->setExpanding(false);
    sections->setDrawBase(false);
    sections->setUsesScrollButtons(true);
    sections->setElideMode(Qt::ElideNone);
    navigationLayout->addWidget(sections);
    layout->addWidget(navigation);
    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    auto *body = new QWidget(scroll);
    body->setObjectName("configBody");
    auto *cards = column(body, 20, 16);
    auto section = [&](const QString &name, const QString &caption) {
        auto *card = new QWidget(body);
        card->setObjectName(name);
        card->setProperty("configCard", true);
        card->setAttribute(Qt::WA_StyledBackground, true);
        auto *content = column(card, 18, 12);
        auto *title = label(caption, card);
        title->setProperty("heading", true);
        content->addWidget(title);
        cards->addWidget(card);
        card->setVisible(sections->addTab(caption) == 0);
        return content;
    };
    QSettings settings;
    auto *general = section("generalSettingsCard", QCoreApplication::translate("MainWindow", "常用设置"));
    auto *fields = new QFormLayout;
    fields->setSpacing(12);
    fields->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    fields->setRowWrapPolicy(QFormLayout::WrapLongRows);
    fields->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    auto *language = new QComboBox(body);
    language->setObjectName("uiLanguage");
    language->addItem(QCoreApplication::translate("MainWindow", "跟随系统"), "system");
    language->addItem("简体中文", "zh_CN");
    language->addItem("English", "en_US");
    language->setCurrentIndex(qMax(0, language->findData(settings.value("General/language", "system"))));
    fields->addRow(QCoreApplication::translate("MainWindow", "语言"), language);
    auto *font = new QFontComboBox(body);
    font->setObjectName("uiFontFamily");
    font->setCurrentFont(QFont(settings.value("General/uiFontFamily", this->font().family()).toString()));
    font->setMinimumWidth(0);
    font->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    auto *fontSize = new QSpinBox(body);
    fontSize->setObjectName("uiFontSize");
    fontSize->setRange(10, 24);
    fontSize->setValue(settings.value("General/uiFontSize", 14).toInt());
    fields->addRow(QCoreApplication::translate("MainWindow", "UI 字体"), font);
    fields->addRow(QCoreApplication::translate("MainWindow", "UI 字号（px）"), fontSize);
    general->addLayout(fields);
    auto *tray = new QCheckBox(QCoreApplication::translate("MainWindow", "关闭时最小化到系统托盘"), body);
    tray->setObjectName("minimizeToTray");
    tray->setChecked(settings.value("General/minimizeToTray", false).toBool());
    general->addWidget(tray);
    general->addWidget(label(QCoreApplication::translate("MainWindow", "托盘菜单可恢复窗口或退出；系统没有可用托盘时正常退出。"), body, true));
    auto *updates = new QCheckBox(QCoreApplication::translate("MainWindow", "启动时检查更新"), body);
    updates->setObjectName("checkUpdatesOnStartup");
    updates->setChecked(settings.value("General/checkUpdatesOnStartup", true).toBool());
    general->addWidget(updates);
    general->addWidget(label(QCoreApplication::translate("MainWindow", "通过 GitHub 检查新版本，有更新时提示下载。"), body, true));
    general->addWidget(label(QCoreApplication::translate("MainWindow", "设置保存后生效。语言设置在重启后生效。"), body, true));

    auto *editorCard = section("editorSettingsCard", QCoreApplication::translate("MainWindow", "编辑器设置"));
    // An empty workspace still allows editing the same settings as an open note.
    auto *settingsEditor = editor;
    if (!settingsEditor) {
        settingsEditor = new MarkdownEditor(this);
        settingsEditor->hide();
    }
    std::function<void()> applyEditor;
    editorCard->addWidget(label(QCoreApplication::translate("MainWindow", "应用于所有笔记。"), body, true));
    editorCard->addWidget(settingsEditor->createSettingsPage(body, applyEditor));

    auto *about = section("aboutSettingsCard", QCoreApplication::translate("MainWindow", "关于"));
    about->addWidget(label("qsnote", body));
    about->addWidget(label(QCoreApplication::translate("MainWindow", "版本 %1").arg(QCoreApplication::applicationVersion()), body, true));
    about->addWidget(label(QCoreApplication::translate("MainWindow", "本地 Markdown 笔记、静态站点、电子书与待办管理。"), body, true));
    auto *check = button(QCoreApplication::translate("MainWindow", "检查更新"), "checkUpdates", body);
    about->addWidget(check, 0, Qt::AlignLeft);
    auto *updateStatus = label("", body, true);
    updateStatus->setObjectName("updateStatus");
    updateStatus->setTextFormat(Qt::PlainText);
    about->addWidget(updateStatus);
    connect(check, &QPushButton::clicked, this, [this, updateStatus] { emit updateCheckRequested(updateStatus); });
    auto *project = label(QCoreApplication::translate("MainWindow", "<a href=\"https://github.com/qiushao/qsnote\">项目主页</a> · <a href=\"https://github.com/qiushao/qsnote/releases/latest\">版本下载</a>"), body);
    project->setOpenExternalLinks(true);
    about->addWidget(project);
    cards->addStretch();
    scroll->setWidget(body);
    layout->addWidget(scroll, 1);
    connect(sections, &QTabBar::currentChanged, this, [cards, scroll](int index) {
        for (int i = 0; i < cards->count() - 1; ++i) cards->itemAt(i)->widget()->setVisible(i == index);
        scroll->verticalScrollBar()->setValue(0);
    });
    auto *footer = new QWidget(this);
    footer->setObjectName("dialogFooter");
    auto *footerLayout = row(footer, 16, 12);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, footer);
    buttons->button(QDialogButtonBox::Save)->setText(QCoreApplication::translate("MainWindow", "保存"));
    setPrimaryButton(buttons->button(QDialogButtonBox::Save));
    buttons->button(QDialogButtonBox::Cancel)->setText(QCoreApplication::translate("MainWindow", "取消"));
    footerLayout->addWidget(buttons);
    layout->addWidget(footer);
    connect(close, &QPushButton::clicked, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    apply_ = [language, font, fontSize, tray, updates, applyEditor] {
        QSettings settings;
        settings.setValue("General/language", language->currentData());
        settings.setValue("General/uiFontFamily", font->currentFont().family());
        settings.setValue("General/uiFontSize", fontSize->value());
        settings.setValue("General/minimizeToTray", tray->isChecked());
        settings.setValue("General/checkUpdatesOnStartup", updates->isChecked());
        settings.sync();
        applyEditor();
    };
}

void SettingsDialog::accept() {
    apply_();
    OverlayDialog::accept();
}
