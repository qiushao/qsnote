#include "SettingsDialog.h"
#include "FlowLayout.h"
#include "MarkdownEditor.h"
#include "Theme.h"
#include "UiStyle.h"
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QFontComboBox>
#include <QFormLayout>
#include <QHash>
#include <QListWidget>
#include <QPainter>
#include <QPainterPath>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QSpinBox>
#include <QToolButton>

namespace {

// 主题模式图标（太阳/月亮/显示器），按当前主题色或灰色绘制
QPixmap themeModeIcon(const QString &mode, const QColor &color) {
    constexpr int size = 48;
    constexpr double pi = 3.14159265358979323846;
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    QPen pen(color, 3.4, Qt::SolidLine, Qt::RoundCap);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    if (mode == QLatin1String("light")) {
        const QPointF center(size / 2.0, size / 2.0);
        painter.drawEllipse(center, 8.0, 8.0);
        for (int i = 0; i < 8; ++i) {
            const double angle = i * pi / 4.0;
            painter.drawLine(QPointF(center.x() + 12 * cos(angle), center.y() + 12 * sin(angle)),
                             QPointF(center.x() + 16.5 * cos(angle), center.y() + 16.5 * sin(angle)));
        }
    } else if (mode == QLatin1String("dark")) {
        QPainterPath outer;
        outer.addEllipse(QRectF(8, 7, 31, 31));
        QPainterPath inner;
        inner.addEllipse(QRectF(19, 2, 31, 31));
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawPath(outer.subtracted(inner));
    } else {
        painter.drawRoundedRect(QRectF(7, 9, 34, 23), 3, 3);
        painter.drawLine(QPointF(24, 32), QPointF(24, 38));
        painter.drawLine(QPointF(16, 39), QPointF(32, 39));
    }
    return pixmap;
}

// 主题色色板，选中时叠加白色对勾
QPixmap accentSwatchIcon(const QColor &color, bool checked) {
    constexpr int size = 56;
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawEllipse(QRectF(2, 2, size - 4, size - 4));
    if (checked) {
        painter.setPen(QPen(Qt::white, 5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPolyline(QPolygonF{{18, 29}, {25, 36}, {38, 20}});
    }
    return pixmap;
}

}// namespace

SettingsDialog::SettingsDialog(MarkdownEditor *editor, QWidget *parent) : OverlayDialog(parent, "settingsDialog", true) {
    resize(qMin(960, parent->width() * 92 / 100), height());
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
    auto *contentPane = new QWidget(this);
    auto *contentLayout = row(contentPane, 0, 0, 0);
    auto *sections = new QListWidget(contentPane);
    sections->setObjectName("settingsNavigation");
    sections->setFixedWidth(qBound(112, width() / 5, 192));
    sections->setWordWrap(true);
    sections->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    contentLayout->addWidget(sections);
    layout->addWidget(contentPane, 1);
    auto *scroll = new QScrollArea(contentPane);
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
        sections->addItem(caption);
        card->setVisible(sections->count() == 1);
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

    auto *appearance = section("appearanceSettingsCard", QCoreApplication::translate("MainWindow", "外观"));
    const QColor iconDim = Theme::color("textDim");
    const QColor accentNow = Theme::accentColor();
    appearance->addWidget(label(QCoreApplication::translate("MainWindow", "主题模式"), body, true));
    auto *modes = new QWidget(body);
    modes->setObjectName("themeModes");
    auto *modeLayout = row(modes, 0, 0, 12);
    auto *modeGroup = new QButtonGroup(modes);
    const QString currentMode = settings.value("Appearance/mode", "system").toString();
    const QList<QPair<QString, QString>> modeDefs = {{"light", QCoreApplication::translate("MainWindow", "浅色")},
                                                     {"dark", QCoreApplication::translate("MainWindow", "深色")},
                                                     {"system", QCoreApplication::translate("MainWindow", "跟随系统")}};
    for (const auto &entry: modeDefs) {
        auto *card = new QToolButton(modes);
        card->setObjectName("themeMode_" + entry.first);
        card->setProperty("modeCard", true);
        card->setProperty("mode", entry.first);
        card->setText(entry.second);
        card->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        card->setCheckable(true);
        card->setChecked(entry.first == currentMode);
        card->setCursor(Qt::PointingHandCursor);
        QIcon icon;
        icon.addPixmap(themeModeIcon(entry.first, iconDim), QIcon::Normal, QIcon::Off);
        icon.addPixmap(themeModeIcon(entry.first, accentNow), QIcon::Normal, QIcon::On);
        card->setIcon(icon);
        card->setIconSize({24, 24});
        card->setMinimumHeight(66);
        card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        modeLayout->addWidget(card);
        modeGroup->addButton(card);
    }
    appearance->addWidget(modes);
    appearance->addWidget(label(QCoreApplication::translate("MainWindow", "主题色"), body, true));
    auto *swatches = new QWidget(body);
    swatches->setObjectName("accentSwatches");
    auto *swatchLayout = new FlowLayout(swatches, 0, 12);
    auto *swatchGroup = new QButtonGroup(swatches);
    const QHash<QString, QString> accentNames = {
            {"green", QCoreApplication::translate("MainWindow", "春山翠")},
            {"rose", QCoreApplication::translate("MainWindow", "霜叶红")},
            {"sakura", QCoreApplication::translate("MainWindow", "晚樱粉")},
            {"sunset", QCoreApplication::translate("MainWindow", "日落暖")},
            {"amber", QCoreApplication::translate("MainWindow", "麦浪金")},
            {"cyan", QCoreApplication::translate("MainWindow", "雨过青")},
            {"blue", QCoreApplication::translate("MainWindow", "秋水蓝")},
            {"purple", QCoreApplication::translate("MainWindow", "暮山紫")},
            {"ink", QCoreApplication::translate("MainWindow", "远山墨")}};
    const QString currentAccent = settings.value("Appearance/accent", "sunset").toString();
    for (const auto &id: Theme::accentIds()) {
        const QColor color = Theme::accentColor(id, false);
        auto *item = new QWidget(swatches);
        auto *itemLayout = column(item, 0, 4);
        auto *swatch = new QPushButton(item);
        swatch->setObjectName("accentSwatch");
        swatch->setCheckable(true);
        swatch->setChecked(id == currentAccent);
        swatch->setProperty("accent", id);
        swatch->setToolTip(accentNames.value(id));
        swatch->setCursor(Qt::PointingHandCursor);
        QIcon icon;
        icon.addPixmap(accentSwatchIcon(color, false), QIcon::Normal, QIcon::Off);
        icon.addPixmap(accentSwatchIcon(color, true), QIcon::Normal, QIcon::On);
        swatch->setIcon(icon);
        swatch->setIconSize({28, 28});
        swatch->setFixedSize(34, 34);
        auto *name = label(accentNames.value(id), item);
        name->setAlignment(Qt::AlignCenter);
        if (id == currentAccent) name->setStyleSheet("color: " + color.name());
        connect(swatch, &QPushButton::toggled, name, [name, color](bool on) { name->setStyleSheet(on ? "color: " + color.name() : QString()); });
        itemLayout->addWidget(swatch, 0, Qt::AlignCenter);
        itemLayout->addWidget(name);
        swatchLayout->addWidget(item);
        swatchGroup->addButton(swatch);
    }
    appearance->addWidget(swatches);

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
    contentLayout->addWidget(scroll, 1);
    sections->setCurrentRow(0);
    connect(sections, &QListWidget::currentRowChanged, this, [cards, scroll](int index) {
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
    apply_ = [language, font, fontSize, tray, updates, modeGroup, swatchGroup, applyEditor] {
        QSettings settings;
        settings.setValue("General/language", language->currentData());
        settings.setValue("General/uiFontFamily", font->currentFont().family());
        settings.setValue("General/uiFontSize", fontSize->value());
        settings.setValue("General/minimizeToTray", tray->isChecked());
        settings.setValue("General/checkUpdatesOnStartup", updates->isChecked());
        if (auto *checked = modeGroup->checkedButton()) settings.setValue("Appearance/mode", checked->property("mode").toString());
        if (auto *checked = swatchGroup->checkedButton()) settings.setValue("Appearance/accent", checked->property("accent").toString());
        settings.sync();
        applyEditor();
    };
}

void SettingsDialog::accept() {
    apply_();
    OverlayDialog::accept();
}
