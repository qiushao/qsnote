#include "SiteContentSettings.h"
#include "SiteNavigationIconPicker.h"
#include "UiStyle.h"
#include "core/SiteDocuments.h"
#include "core/SiteNavigationIcons.h"
#include <QIcon>
#include <QCheckBox>
#include <QCoreApplication>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QLineEdit>
#include <QMessageBox>
#include <QTreeWidget>

namespace {
QIcon navigationIcon(const QJsonObject &item) {
    const auto path = SiteNavigationIcons::resource(item["icon"].toString());
    if (!path.isEmpty()) return QIcon(path);
    QPixmap image;
    const auto svg = SiteNavigationIcons::markup(item, "tree-").replace("currentColor", "#6e6e7b");
    if (!svg.isEmpty()) image.loadFromData(svg.toUtf8(), "SVG");
    return {image};
}
QString navigationTarget(const QJsonObject &item, const Notebook &book) {
    const auto type = item["type"].toString(), target = item["target"].toString();
    if (type == "group") return QCoreApplication::translate("SiteContentSettings", "分组");
    if (type == "collection") return SiteDocuments::collection(book, target)["name"].toString(target);
    if (type == "note") {
        const auto *note = SiteDocuments::find(book, target);
        return note ? note->title : target;
    }
    return target.isEmpty() ? QCoreApplication::translate("SiteContentSettings", "首页") : target;
}

bool editCollection(QJsonObject &item, bool creating, QWidget *parent) {
    QDialog dialog(parent);
    dialog.setWindowTitle(QCoreApplication::translate("SiteContentSettings", "文档集合"));
    auto *form = new QFormLayout(&dialog);
    auto *name = new QLineEdit(item["name"].toString(), &dialog);
    auto *slug = new QLineEdit(item["slug"].toString(), &dialog);
    slug->setReadOnly(!creating);
    form->addRow(QCoreApplication::translate("SiteContentSettings", "名称"), name);
    form->addRow("Slug", slug);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form->addRow(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        if (name->text().trimmed().isEmpty() || !SiteDocuments::validSlug(slug->text().trimmed())) {
            QMessageBox::warning(&dialog, QCoreApplication::translate("SiteContentSettings", "配置无效"), QCoreApplication::translate("SiteContentSettings", "名称不能为空；Slug 必须以字母或数字开头，只能包含字母、数字、下划线和连字符。"));
            return;
        }
        dialog.accept();
    });
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return false;
    item["name"] = name->text().trimmed();
    item["slug"] = slug->text().trimmed();
    return true;
}

bool editNavigation(QJsonObject &item, const Notebook &book, QWidget *parent) {
    QDialog dialog(parent);
    dialog.setWindowTitle(QCoreApplication::translate("SiteContentSettings", "导航项"));
    dialog.resize(640, 440);
    auto *form = new QFormLayout(&dialog);
    auto *title = new QLineEdit(item["title"].toString(), &dialog);
    form->addRow(QCoreApplication::translate("SiteContentSettings", "名称"), title);
    auto *icon = new SiteNavigationIconPicker(item, &dialog);
    form->addRow(QCoreApplication::translate("SiteContentSettings", "图标"), icon);
    auto *target = new QComboBox(&dialog);
    auto *path = new QLineEdit(item["target"].toString(), &dialog);
    form->addRow(QCoreApplication::translate("SiteContentSettings", "目标"), target);
    form->addRow(QCoreApplication::translate("SiteContentSettings", "路径或 HTTP(S) 网址"), path);
    const auto add = [target](const QString &label, const QString &type, const QString &value) {
        target->addItem(label, QJsonObject{{"type", type}, {"target", value}});
    };
    if (item["type"] == QJsonValue("group")) add(QCoreApplication::translate("SiteContentSettings", "分组"), "group", {});
    else {
        for (const auto &entry: QList<QPair<QString, QString>>{{"", QCoreApplication::translate("SiteContentSettings", "首页")}, {"categories/", QCoreApplication::translate("SiteContentSettings", "分类")}, {"tags/", QCoreApplication::translate("SiteContentSettings", "标签")}, {"archives/", QCoreApplication::translate("SiteContentSettings", "归档")}, {"links/", QCoreApplication::translate("SiteContentSettings", "友链")}, {"about/", QCoreApplication::translate("SiteContentSettings", "关于")}}) add(entry.second, "builtin", entry.first);
        for (const auto &value: book.config["docCollections"].toArray()) {
            const auto group = value.toObject();
            add(QCoreApplication::translate("SiteContentSettings", "集合：") + group["name"].toString(), "collection", group["id"].toString());
        }
        for (const auto &note: book.notes) if (!note.folder && !note.metadata["draft"].toBool()) add(note.title, "note", note.id);
        add(QCoreApplication::translate("SiteContentSettings", "站内路径"), "path", {});
        add(QCoreApplication::translate("SiteContentSettings", "外部链接"), "external", {});
    }
    for (int i = 0; i < target->count(); ++i) {
        const auto value = target->itemData(i).toJsonObject();
        if (value["type"] == item["type"] && (value["target"] == item["target"] || value["type"] == QJsonValue("path") || value["type"] == QJsonValue("external"))) target->setCurrentIndex(i);
    }
    const auto update = [target, path] {
        const auto type = target->currentData().toJsonObject()["type"].toString();
        path->setEnabled(type == "path" || type == "external");
    };
    QObject::connect(target, &QComboBox::currentIndexChanged, &dialog, update);
    update();
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form->addRow(buttons);
    const auto updateAccept = [buttons, title, icon] { buttons->button(QDialogButtonBox::Ok)->setEnabled(!title->text().trimmed().isEmpty() && icon->isValid()); };
    QObject::connect(title, &QLineEdit::textChanged, &dialog, updateAccept);
    QObject::connect(icon, &SiteNavigationIconPicker::validityChanged, &dialog, updateAccept);
    updateAccept();
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] { if (!title->text().trimmed().isEmpty() && icon->isValid()) dialog.accept(); });
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted || !icon->isValid()) return false;
    const auto value = target->currentData().toJsonObject();
    item["title"] = title->text().trimmed();
    icon->apply(item);
    item["type"] = value["type"];
    item["target"] = path->isEnabled() ? path->text().trimmed() : value["target"].toString();
    return true;
}
}

QWidget *siteContentSettings(Notebook &book, QWidget *parent) {
    auto *widget = new QWidget(parent);
    auto *layout = column(widget, 0, 12);
    auto *collections = new QTreeWidget(widget);
    collections->setHeaderLabels({QCoreApplication::translate("SiteContentSettings", "文档集合"), "Slug"});
    collections->setRootIsDecorated(false);
    collections->setMinimumHeight(160);
    const auto appendCollection = [collections](const QJsonObject &item) {
        auto *row = new QTreeWidgetItem(collections, {item["name"].toString(), item["slug"].toString()});
        row->setData(0, Qt::UserRole, item);
    };
    for (const auto &item: book.config["docCollections"].toArray()) appendCollection(item.toObject());
    layout->addWidget(collections);
    const auto saveCollections = [&book, collections] {
        QJsonArray items;
        for (int i = 0; i < collections->topLevelItemCount(); ++i) items.append(collections->topLevelItem(i)->data(0, Qt::UserRole).toJsonObject());
        book.config["docCollections"] = items;
    };
    auto *collectionActions = new QWidget(widget);
    auto *collectionRow = row(collectionActions, 0, 0);
    for (const auto &action: {QCoreApplication::translate("SiteContentSettings", "新增集合"), QCoreApplication::translate("SiteContentSettings", "改名"), QCoreApplication::translate("SiteContentSettings", "删除"), QCoreApplication::translate("SiteContentSettings", "上移"), QCoreApplication::translate("SiteContentSettings", "下移")}) {
        auto *control = new QPushButton(action, collectionActions);
        collectionRow->addWidget(control);
        QObject::connect(control, &QPushButton::clicked, widget, [&, widget, collections, appendCollection, saveCollections, action] {
            auto *selected = collections->currentItem();
            if (action == QCoreApplication::translate("SiteContentSettings", "新增集合")) {
                const auto id = NotebookStore::newId();
                QJsonObject item{{"id", id}, {"slug", "docs-" + id.left(8)}};
                if (!editCollection(item, true, widget)) return;
                for (const auto &value: book.config["docCollections"].toArray()) if (value.toObject()["slug"].toString().compare(item["slug"].toString(), Qt::CaseInsensitive) == 0) {
                    QMessageBox::warning(widget, QCoreApplication::translate("SiteContentSettings", "配置无效"), QCoreApplication::translate("SiteContentSettings", "集合 Slug 已存在。"));
                    return;
                }
                appendCollection(item);
            } else if (selected) {
                auto item = selected->data(0, Qt::UserRole).toJsonObject();
                if (action == QCoreApplication::translate("SiteContentSettings", "改名")) {
                    if (!editCollection(item, false, widget)) return;
                    selected->setText(0, item["name"].toString());
                    selected->setData(0, Qt::UserRole, item);
                } else if (action == QCoreApplication::translate("SiteContentSettings", "删除")) {
                    for (const auto &note: book.notes) if (SiteDocuments::isDocument(note) && note.metadata["collectionId"] == item["id"]) {
                        QMessageBox::warning(widget, QCoreApplication::translate("SiteContentSettings", "无法删除"), QCoreApplication::translate("SiteContentSettings", "只能删除空集合。"));
                        return;
                    }
                    delete selected;
                } else {
                    const auto index = collections->indexOfTopLevelItem(selected);
                    const auto destination = index + (action == QCoreApplication::translate("SiteContentSettings", "上移") ? -1 : 1);
                    if (destination < 0 || destination >= collections->topLevelItemCount()) return;
                    collections->takeTopLevelItem(index);
                    collections->insertTopLevelItem(destination, selected);
                    collections->setCurrentItem(selected);
                }
            }
            saveCollections();
        });
    }
    layout->addWidget(collectionActions);
    auto *custom = new QCheckBox(QCoreApplication::translate("SiteContentSettings", "自定义导航（仅 Plume 展示）"), widget);
    custom->setChecked(book.config.contains("navigation"));
    layout->addWidget(custom);
    auto *navigation = new QTreeWidget(widget);
    navigation->setHeaderLabels({QCoreApplication::translate("SiteContentSettings", "导航"), QCoreApplication::translate("SiteContentSettings", "目标")});
    navigation->setObjectName("siteNavigationTree");
    navigation->setMinimumHeight(220);
    navigation->setEnabled(custom->isChecked());
    const auto appendNavigation = [navigation, &book](const QJsonObject &item, QTreeWidgetItem *parentItem) {
        auto *node = new QTreeWidgetItem({item["title"].toString(), navigationTarget(item, book)});
        node->setIcon(0, navigationIcon(item));
        auto data = item;
        data.remove("children");
        node->setData(0, Qt::UserRole, data);
        if (parentItem) parentItem->addChild(node);
        else navigation->addTopLevelItem(node);
        return node;
    };
    for (const auto &entry: book.config["navigation"].toArray()) {
        const auto item = entry.toObject();
        auto *node = appendNavigation(item, nullptr);
        for (const auto &child: item["children"].toArray()) appendNavigation(child.toObject(), node);
    }
    navigation->expandAll();
    layout->addWidget(navigation);
    const auto saveNavigation = [&book, navigation, custom] {
        if (!custom->isChecked()) { book.config.remove("navigation"); return; }
        QJsonArray items;
        for (int i = 0; i < navigation->topLevelItemCount(); ++i) {
            const auto *node = navigation->topLevelItem(i);
            auto item = node->data(0, Qt::UserRole).toJsonObject();
            if (item["type"] == QJsonValue("group")) {
                QJsonArray children;
                for (int j = 0; j < node->childCount(); ++j) children.append(node->child(j)->data(0, Qt::UserRole).toJsonObject());
                item["children"] = children;
            }
            items.append(item);
        }
        book.config["navigation"] = items;
    };
    QObject::connect(custom, &QCheckBox::toggled, widget, [navigation, saveNavigation](bool checked) { navigation->setEnabled(checked); saveNavigation(); });
    auto *navigationActions = new QWidget(widget);
    auto *navigationRow = row(navigationActions, 0, 0);
    for (const auto &action: {QCoreApplication::translate("SiteContentSettings", "新增链接"), QCoreApplication::translate("SiteContentSettings", "新增分组"), QCoreApplication::translate("SiteContentSettings", "编辑"), QCoreApplication::translate("SiteContentSettings", "删除"), QCoreApplication::translate("SiteContentSettings", "上移"), QCoreApplication::translate("SiteContentSettings", "下移")}) {
        auto *control = new QPushButton(action, navigationActions);
        navigationRow->addWidget(control);
        QObject::connect(control, &QPushButton::clicked, widget, [&, widget, navigation, custom, appendNavigation, saveNavigation, action] {
            if (!custom->isChecked()) return;
            auto *selected = navigation->currentItem();
            if (action == QCoreApplication::translate("SiteContentSettings", "新增链接") || action == QCoreApplication::translate("SiteContentSettings", "新增分组")) {
                QJsonObject item{{"type", action == QCoreApplication::translate("SiteContentSettings", "新增分组") ? "group" : "builtin"}, {"target", ""}};
                if (!editNavigation(item, book, widget)) return;
                auto *parentItem = selected && selected->data(0, Qt::UserRole).toJsonObject()["type"] == QJsonValue("group") && action == QCoreApplication::translate("SiteContentSettings", "新增链接") ? selected : nullptr;
                appendNavigation(item, parentItem);
                navigation->expandAll();
            } else if (selected) {
                if (action == QCoreApplication::translate("SiteContentSettings", "编辑")) {
                    auto item = selected->data(0, Qt::UserRole).toJsonObject();
                    if (!editNavigation(item, book, widget)) return;
                    selected->setText(0, item["title"].toString());
                    selected->setIcon(0, navigationIcon(item));
                    selected->setText(1, navigationTarget(item, book));
                    selected->setData(0, Qt::UserRole, item);
                } else if (action == QCoreApplication::translate("SiteContentSettings", "删除")) delete selected;
                else {
                    auto *parentItem = selected->parent() ? selected->parent() : navigation->invisibleRootItem();
                    const auto index = parentItem->indexOfChild(selected);
                    const auto destination = index + (action == QCoreApplication::translate("SiteContentSettings", "上移") ? -1 : 1);
                    if (destination < 0 || destination >= parentItem->childCount()) return;
                    parentItem->takeChild(index);
                    parentItem->insertChild(destination, selected);
                    navigation->setCurrentItem(selected);
                }
            }
            saveNavigation();
        });
    }
    layout->addWidget(navigationActions);
    layout->addWidget(label(QCoreApplication::translate("SiteContentSettings", "选中分组后新增链接可添加子项。取消自定义导航恢复默认项。集合 Slug 创建后固定；导航引用草稿或已删除内容时需修正。"), widget, true));
    auto *plumeLink = label(QCoreApplication::translate("SiteContentSettings", "<a href=\"https://theme-plume.vuejs.press/blog/\">Plume 官方网站</a>"), widget, true);
    plumeLink->setOpenExternalLinks(true);
    layout->addWidget(plumeLink);
    return widget;
}
