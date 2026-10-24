#include "SiteConfigDialog.h"
#include "UiStyle.h"
#include "core/NotebookCategories.h"
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QImage>
#include <QInputDialog>
#include <QJsonArray>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QSaveFile>
#include <QScrollArea>
#include <QScrollBar>

SiteConfigDialog::SiteConfigDialog(Notebook &notebook, QWidget *parent) : OverlayDialog(parent, "siteConfigDialog", true) {
    auto *book = &notebook;
    auto &dialog = *this;
    dialog.resize(qMin(960, parent->width() * 92 / 100), dialog.height());
    auto *layout = column(&dialog);
    auto *bar = new QWidget(&dialog);
    bar->setObjectName("dialogBar");
    auto *barLayout = row(bar, 16, 11);
    auto *title = label(QCoreApplication::translate("MainWindow", "站点配置"), bar);
    title->setProperty("dialogTitle", true);
    barLayout->addWidget(title);
    barLayout->addStretch();
    auto *close = button(QCoreApplication::translate("MainWindow", "关闭"), "closeSiteConfig", bar);
    barLayout->addWidget(close);
    layout->addWidget(bar);
    auto *content = new QWidget(&dialog);
    auto *contentLayout = row(content, 0, 0, 0);
    auto *sections = new QListWidget(content);
    sections->setObjectName("siteConfigNavigation");
    sections->setFixedWidth(qBound(112, dialog.width() / 5, 192));
    sections->setWordWrap(true);
    sections->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    contentLayout->addWidget(sections);
    layout->addWidget(content, 1);
    auto *scroll = new QScrollArea(content);
    scroll->setObjectName("siteConfigScroll");
    scroll->setWidgetResizable(true);
    auto *body = new QWidget(scroll);
    body->setObjectName("siteConfigBody");
    auto *cards = column(body, 20, 16);
    QWidget *card = nullptr;
    QVBoxLayout *form = nullptr;
    auto section = [&](const QString &name, const QString &text) {
        card = new QWidget(body);
        card->setObjectName(name);
        card->setProperty("configCard", true);
        card->setAttribute(Qt::WA_StyledBackground, true);
        form = column(card, 18, 12);
        auto *title = label(text, card);
        title->setProperty("heading", true);
        form->addWidget(title);
        cards->addWidget(card);
        sections->addItem(text);
        card->setVisible(sections->count() == 1);
    };
    auto field = [&](const QString &key, const QString &caption, const QStringList &choices = QStringList(), const QStringList &values = QStringList()) {
        auto *container = new QWidget(card);
        auto *fieldLayout = new QFormLayout(container);
        fieldLayout->setContentsMargins(0, 0, 0, 0);
        fieldLayout->setHorizontalSpacing(16);
        fieldLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        fieldLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        fieldLayout->setRowWrapPolicy(QFormLayout::WrapLongRows);
        auto *captionLabel = label(caption, container, true);
        captionLabel->setMinimumWidth(96);
        captionLabel->setWordWrap(false);
        if (choices.isEmpty()) {
            auto *input = new QLineEdit(book->config[key].toString(), container);
            input->setObjectName("config_" + key);
            input->setMinimumWidth(0);
            if (key == "clientSecret") input->setEchoMode(QLineEdit::Password);
            captionLabel->setBuddy(input);
            fieldLayout->addRow(captionLabel, input);
            connect(input, &QLineEdit::textEdited, &dialog, [this, book, key](const QString &value) {
                book->config[key] = value;
                emit saveRequested(true);
            });
        } else {
            auto *select = new QComboBox(container);
            select->setObjectName("config_" + key);
            for (int index = 0; index < choices.size(); ++index) select->addItem(choices[index], values.value(index, choices[index]));
            select->setCurrentIndex(qMax(0, select->findData(book->config[key].toString())));
            captionLabel->setBuddy(select);
            fieldLayout->addRow(captionLabel, select);
            connect(select, &QComboBox::currentIndexChanged, &dialog, [this, book, key, select] {
                book->config[key] = select->currentData().toString();
                emit saveRequested(true);
            });
        }
        form->addWidget(container);
        return container;
    };
    section("siteBasicCard", QCoreApplication::translate("MainWindow", "基本信息"));
    field("title", QCoreApplication::translate("MainWindow", "站点标题"));
    field("description", QCoreApplication::translate("MainWindow", "站点描述"));
    field("keywords", QCoreApplication::translate("MainWindow", "SEO 关键词"))->findChild<QLineEdit *>()->setPlaceholderText(QCoreApplication::translate("MainWindow", "逗号分隔"));
    field("author", QCoreApplication::translate("MainWindow", "作者"));
    field("github", QCoreApplication::translate("MainWindow", "GitHub 地址"))->findChild<QLineEdit *>()->setPlaceholderText("https://github.com/username");
    field("city", QCoreApplication::translate("MainWindow", "城市"));
    auto *avatarField = field("avatar", QCoreApplication::translate("MainWindow", "头像"));
    auto *avatar = avatarField->findChild<QLineEdit *>();
    avatar->setPlaceholderText(QCoreApplication::translate("MainWindow", "图片网址或 source 内相对路径；留空使用默认头像"));
    avatar->setClearButtonEnabled(true);
    auto *chooseAvatar = button(QCoreApplication::translate("MainWindow", "选择图片…"), "chooseSiteAvatar", avatarField);
    qobject_cast<QFormLayout *>(avatarField->layout())->addRow(QString(), chooseAvatar);
    connect(chooseAvatar, &QPushButton::clicked, &dialog, [this, book, avatar, &dialog] {
        const auto selected = QFileDialog::getOpenFileName(&dialog, QCoreApplication::translate("MainWindow", "选择头像"), book->path,
                                                           QCoreApplication::translate("MainWindow", "图片 (*.png *.jpg *.jpeg *.webp *.bmp)"));
        if (selected.isEmpty()) return;
        const QImage image(selected);
        if (image.isNull()) {
            QMessageBox::warning(&dialog, QCoreApplication::translate("MainWindow", "头像设置失败"), QCoreApplication::translate("MainWindow", "无法读取所选图片。"));
            return;
        }
        const auto relative = QStringLiteral("images/qsnote-avatar.png");
        const auto target = book->path + "/source/" + relative;
        QSaveFile file(target);
        if (!QDir().mkpath(QFileInfo(target).absolutePath()) || !file.open(QIODevice::WriteOnly) || !image.save(&file, "PNG") || !file.commit()) {
            QMessageBox::warning(&dialog, QCoreApplication::translate("MainWindow", "头像设置失败"), QCoreApplication::translate("MainWindow", "无法保存头像：%1").arg(target));
            return;
        }
        avatar->setText(relative);
        book->config["avatar"] = relative;
        emit saveRequested(true);
    });
    field("domain", QCoreApplication::translate("MainWindow", "域名"));
    section("siteCommentsCard", QCoreApplication::translate("MainWindow", "评论"));
    auto *comments = field("commentPlatform", QCoreApplication::translate("MainWindow", "评论平台"), {QCoreApplication::translate("MainWindow", "关闭"), "Valine", "Gitalk", "Disqus"}, {"none", "valine", "gitalk", "disqus"})->findChild<QComboBox *>();
    auto commentField = [&](const QString &key, const QString &caption, const QString &platform) {
        auto *container = field(key, caption);
        container->setVisible(comments->currentData().toString() == platform);
        connect(comments, &QComboBox::currentIndexChanged, container, [container, comments, platform] {
            container->setVisible(comments->currentData().toString() == platform);
        });
    };
    commentField("commentOwner", QCoreApplication::translate("MainWindow", "GitHub 用户"), "gitalk");
    commentField("commentRepo", QCoreApplication::translate("MainWindow", "评论仓库"), "gitalk");
    commentField("clientId", "Client ID", "gitalk");
    commentField("clientSecret", "Client Secret", "gitalk");
    commentField("disqus", "Disqus Shortname", "disqus");
    commentField("valineAppId", "Valine App ID", "valine");
    commentField("valineAppKey", "Valine App Key", "valine");
    section("siteThemeCard", QCoreApplication::translate("MainWindow", "主题与页脚"));
    auto *theme = field("theme", QCoreApplication::translate("MainWindow", "站点主题"), {"Pure", "NexT", "Yilia", "3-Jekyll", QCoreApplication::translate("MainWindow", "自定义")},
                        {"pure", "next", "yilia", "3-jekyll", "custom"})
                          ->findChild<QComboBox *>();
    auto *themeDirectoryField = field("themeDirectory", QCoreApplication::translate("MainWindow", "主题目录"));
    auto *themeDirectory = themeDirectoryField->findChild<QLineEdit *>();
    themeDirectory->setPlaceholderText(QCoreApplication::translate("MainWindow", "站点内相对路径或完整路径"));
    auto *chooseTheme = button(QCoreApplication::translate("MainWindow", "选择主题目录…"), "chooseSiteTheme", card);
    form->addWidget(chooseTheme, 0, Qt::AlignLeft);
    auto *themeHint = label(QCoreApplication::translate("MainWindow", "使用含 layout/*.html 和 source/ 的适配主题。支持 QSNote 占位符；Hexo 的 EJS、Nunjucks 和样式预处理文件需先转换，不执行主题插件或读取 _config.yml。"), card, true);
    themeHint->setWordWrap(true);
    form->addWidget(themeHint);
    const auto updateThemeFields = [theme, themeDirectoryField, chooseTheme, themeHint] {
        const bool custom = theme->currentData().toString() == "custom";
        themeDirectoryField->setVisible(custom);
        chooseTheme->setVisible(custom);
        themeHint->setVisible(custom);
    };
    connect(theme, &QComboBox::currentIndexChanged, &dialog, updateThemeFields);
    updateThemeFields();
    connect(chooseTheme, &QPushButton::clicked, &dialog, [book, themeDirectory, &dialog] {
        const auto path = QFileDialog::getExistingDirectory(&dialog, QCoreApplication::translate("MainWindow", "选择主题目录"),
                                                            QDir(book->path).absoluteFilePath(themeDirectory->text().trimmed()));
        if (path.isEmpty()) return;
        const auto relative = QDir(book->path).relativeFilePath(path);
        themeDirectory->setText(relative != ".." && !relative.startsWith("../") ? relative : path);
        emit themeDirectory->textEdited(themeDirectory->text());
    });
    field("footer", QCoreApplication::translate("MainWindow", "页脚文字"));
    section("siteCategoriesCard", QCoreApplication::translate("MainWindow", "分类管理"));
    auto *categories = new QListWidget(card);
    categories->setObjectName("siteCategories");
    categories->setMinimumHeight(120);
    categories->setMaximumHeight(180);
    categories->addItems(NotebookCategories::site(*book));
    form->addWidget(categories);
    auto *categoryActions = new QWidget(card);
    auto *categoryLayout = row(categoryActions, 0, 0, 8);
    auto *addCategory = button(QCoreApplication::translate("MainWindow", "新建"), "addSiteCategory", categoryActions);
    auto *renameCategory = button(QCoreApplication::translate("MainWindow", "重命名"), "renameSiteCategory", categoryActions);
    auto *deleteCategory = button(QCoreApplication::translate("MainWindow", "删除"), "deleteSiteCategory", categoryActions);
    categoryLayout->addWidget(addCategory);
    categoryLayout->addWidget(renameCategory);
    categoryLayout->addWidget(deleteCategory);
    categoryLayout->addStretch();
    form->addWidget(categoryActions);
    form->addWidget(label(QCoreApplication::translate("MainWindow", "删除分类后，文章将移入 default；默认分类不可重命名或删除。"), card, true));
    connect(categories, &QListWidget::currentTextChanged, &dialog, [renameCategory, deleteCategory](const QString &name) {
        const bool editable = !name.isEmpty() && name != "default";
        renameCategory->setEnabled(editable);
        deleteCategory->setEnabled(editable);
    });
    categories->setCurrentRow(0);
    const auto updateCategories = [this, book, categories](const QString &selected) {
        categories->clear();
        categories->addItems(NotebookCategories::site(*book));
        categories->setCurrentRow(static_cast<int>(NotebookCategories::site(*book).indexOf(selected)));
        emit saveRequested(false);
        emit categoriesChanged();
    };
    const auto editCategory = [book, categories, updateCategories, &dialog](bool rename) {
        const auto previous = rename ? categories->currentItem()->text() : QString();
        bool accepted = false;
        const auto name = QInputDialog::getText(&dialog, rename ? QCoreApplication::translate("MainWindow", "重命名分类") : QCoreApplication::translate("MainWindow", "新建分类"), QCoreApplication::translate("MainWindow", "分类名称："),
                                                QLineEdit::Normal, previous, &accepted)
                                  .trimmed();
        if (!accepted || name.isEmpty() || name == previous) return;
        QString error;
        const bool changed = rename ? NotebookCategories::rename(*book, previous, name, &error)
                                    : NotebookCategories::create(*book, name, &error);
        if (!changed) {
            QMessageBox::warning(&dialog, QCoreApplication::translate("MainWindow", "分类已存在"), error);
            return;
        }
        updateCategories(name);
    };
    connect(addCategory, &QPushButton::clicked, &dialog, [editCategory] { editCategory(false); });
    connect(renameCategory, &QPushButton::clicked, &dialog, [editCategory] { editCategory(true); });
    connect(deleteCategory, &QPushButton::clicked, &dialog, [book, categories, updateCategories, &dialog] {
        const auto name = categories->currentItem()->text();
        if (QMessageBox::question(&dialog, QCoreApplication::translate("MainWindow", "删除分类"), QCoreApplication::translate("MainWindow", "删除分类「%1」？其中的文章将移入 default，不会删除文章。").arg(name),
                                  QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes) return;
        QString error;
        if (!NotebookCategories::remove(*book, name, &error)) {
            QMessageBox::warning(&dialog, QCoreApplication::translate("MainWindow", "删除分类"), error);
            return;
        }
        updateCategories("default");
    });
    section("siteLinksCard", QCoreApplication::translate("MainWindow", "友链"));
    auto *links = new QWidget(card);
    auto *linksLayout = column(links, 0, 6);
    auto appendLink = [this, book, links, linksLayout](const QJsonObject &value) {
        auto *container = new QWidget(links);
        auto *linkLayout = row(container, 0, 0, 8);
        for (const auto &key: {QStringLiteral("name"), QStringLiteral("url")}) {
            auto *input = new QLineEdit(value[key].toString(), container);
            input->setObjectName(key);
            input->setPlaceholderText(key == "name" ? QCoreApplication::translate("MainWindow", "名称") : "https://");
            input->setMinimumWidth(0);
            linkLayout->addWidget(input, 1);
            connect(input, &QLineEdit::textEdited, links, [this, book, links] {
                QJsonArray entries;
                for (auto *linkRow: links->findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly)) {
                    if (linkRow->isHidden()) continue;
                    entries.append(QJsonObject{{"name", linkRow->findChild<QLineEdit *>("name")->text()}, {"url", linkRow->findChild<QLineEdit *>("url")->text()}});
                }
                book->config["links"] = entries;
                emit saveRequested(true);
            });
        }
        auto *remove = button(QCoreApplication::translate("MainWindow", "删除"), "removeLink", container);
        linkLayout->addWidget(remove);
        connect(remove, &QPushButton::clicked, links, [container] {
            container->hide();
            auto *input = container->findChild<QLineEdit *>("name");
            emit input->textEdited(input->text());
            container->deleteLater();
        });
        linksLayout->addWidget(container);
    };
    for (const auto &entry: book->config["links"].toArray()) appendLink(entry.toObject());
    form->addWidget(links);
    auto *addLink = button(QCoreApplication::translate("MainWindow", "＋ 添加友链"), "addLink", card);
    form->addWidget(addLink, 0, Qt::AlignLeft);
    connect(addLink, &QPushButton::clicked, &dialog, [appendLink] { appendLink({}); });
    section("sitePublishCard", QCoreApplication::translate("MainWindow", "生成与发布"));
    auto *method = field("publishMethod", QCoreApplication::translate("MainWindow", "发布方式"), {QCoreApplication::translate("MainWindow", "请选择"), "GitHub Pages", "SSH SCP"}, {"", "github", "scp"})->findChild<QComboBox *>();
    auto publishField = [&](const QString &key, const QString &caption, const QString &platform, const QString &placeholder) {
        auto *container = field(key, caption);
        container->findChild<QLineEdit *>()->setPlaceholderText(placeholder);
        container->setVisible(method->currentData().toString() == platform);
        connect(method, &QComboBox::currentIndexChanged, container, [container, method, platform] {
            container->setVisible(method->currentData().toString() == platform);
        });
    };
    publishField("githubRepository", QCoreApplication::translate("MainWindow", "仓库地址"), "github", QCoreApplication::translate("MainWindow", "git@github.com:user/repo.git 或 HTTPS 地址"));
    publishField("githubBranch", QCoreApplication::translate("MainWindow", "发布分支"), "github", QCoreApplication::translate("MainWindow", "gh-pages（默认）"));
    publishField("sshHost", QCoreApplication::translate("MainWindow", "SSH 主机"), "scp", QCoreApplication::translate("MainWindow", "example.com / IP / SSH 配置别名"));
    publishField("sshPort", QCoreApplication::translate("MainWindow", "SSH 端口"), "scp", QCoreApplication::translate("MainWindow", "22（默认）"));
    publishField("sshUser", QCoreApplication::translate("MainWindow", "SSH 用户名"), "scp", QCoreApplication::translate("MainWindow", "部署用户"));
    publishField("sshDirectory", QCoreApplication::translate("MainWindow", "远程目录"), "scp", QCoreApplication::translate("MainWindow", "/var/www/site（绝对路径，不含空格）"));
    publishField("sshIdentityFile", QCoreApplication::translate("MainWindow", "私钥文件"), "scp", QCoreApplication::translate("MainWindow", "可选，填写本地完整路径；留空使用 SSH 默认配置"));
    auto *publishHint = label({}, card, true);
    const auto updatePublishHint = [method, publishHint] {
        const auto platform = method->currentData().toString();
        publishHint->setText(platform == "github"
                                     ? QCoreApplication::translate("MainWindow", "使用本机 Git 凭据或 SSH 密钥。发布分支根目录将更新为站点内容；请在 GitHub 仓库 Settings → Pages 中选择该分支的 / (root)。域名请填写实际访问地址，项目站点需包含仓库子路径。")
                             : platform == "scp"
                                     ? QCoreApplication::translate("MainWindow", "使用 SSH 密钥或 ssh-agent；请先在终端连接主机并确认指纹。上传会覆盖同名文件，保留远端其他文件。")
                                     : QCoreApplication::translate("MainWindow", "选择发布方式后填写对应设置。预览无需发布设置。"));
    };
    connect(method, &QComboBox::currentIndexChanged, publishHint, updatePublishHint);
    updatePublishHint();
    form->addWidget(publishHint);
    form->addWidget(label(QCoreApplication::translate("MainWindow", "内置生成器离线生成 public 目录；第三方评论服务需要联网。"), card, true));
    auto codeField = [&](const QString &key, const QString &caption, const QString &hint) {
        auto *captionLabel = label(caption, card);
        form->addWidget(captionLabel);
        auto *input = new QPlainTextEdit(book->config[key].toString(), card);
        input->setObjectName("config_" + key);
        input->setProperty("codeInput", true);
        input->setMinimumWidth(0);
        input->setMinimumHeight(150);
        input->setTabChangesFocus(true);
        input->setPlaceholderText(hint);
        captionLabel->setBuddy(input);
        form->addWidget(input);
        connect(input, &QPlainTextEdit::textChanged, &dialog, [this, book, key, input] {
            book->config[key] = input->toPlainText();
            emit saveRequested(true);
        });
    };
    section("siteAnalyticsCard", QCoreApplication::translate("MainWindow", "站长统计"));
    auto *analyticsHint = label(QCoreApplication::translate("MainWindow", "粘贴统计平台提供的完整 HTML / JavaScript 代码，将原样插入所有页面的 </body> 前。留空关闭统计；预览时也会加载。"), card, true);
    analyticsHint->setTextFormat(Qt::PlainText);
    form->addWidget(analyticsHint);
    codeField("analyticsCode", QCoreApplication::translate("MainWindow", "统计代码"), QCoreApplication::translate("MainWindow", "例如统计平台提供的 <script>…</script>"));
    section("siteCustomCodeCard", QCoreApplication::translate("MainWindow", "自定义代码"));
    auto *customCodeHint = label(QCoreApplication::translate("MainWindow", "代码原样应用于所有主题的生成页面，预览时也会执行。JavaScript 请包含 <script> 标签，CSS 请包含 <style> 标签；留空不插入。"), card, true);
    customCodeHint->setTextFormat(Qt::PlainText);
    form->addWidget(customCodeHint);
    codeField("customHeadCode", QCoreApplication::translate("MainWindow", "Head 代码"), QCoreApplication::translate("MainWindow", "插入 </head> 前，例如 <meta>、<style> 或 <script> 标签"));
    codeField("customBodyCode", QCoreApplication::translate("MainWindow", "Body 底部代码"), QCoreApplication::translate("MainWindow", "插入 </body> 前，位于统计代码之后"));
    cards->addStretch();
    scroll->setWidget(body);
    contentLayout->addWidget(scroll, 1);
    sections->setCurrentRow(0);
    connect(sections, &QListWidget::currentRowChanged, &dialog, [cards, scroll](int index) {
        for (int i = 0; i < cards->count() - 1; ++i) cards->itemAt(i)->widget()->setVisible(i == index);
        scroll->verticalScrollBar()->setValue(0);
    });
    connect(close, &QPushButton::clicked, &dialog, &QDialog::accept);
}
