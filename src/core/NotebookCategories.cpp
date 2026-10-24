#include "NotebookCategories.h"
#include <QCoreApplication>
#include <QJsonArray>

namespace {
QStringList collect(const Notebook &book, const QString &fallback) {
    QStringList names{fallback};
    for (const auto &value: book.config["categories"].toArray()) names << value.toString();
    for (const auto &note: book.notes) names << note.metadata["category"].toString();
    names.removeAll({});
    names.removeDuplicates();
    return names;
}

QString fallback(const Notebook &book) {
    return book.type == "todo" ? QStringLiteral("默认") : QStringLiteral("default");
}

QStringList categories(const Notebook &book) {
    return book.type == "todo" ? NotebookCategories::todo(book) : NotebookCategories::site(book);
}

bool editable(const Notebook &book, const QString &name, QString *error) {
    if (name == fallback(book) || (book.type == "todo" && name == QStringLiteral("全部"))) {
        *error = QCoreApplication::translate("MainWindow", "默认分类不可重命名或删除。");
        return false;
    }
    if (!categories(book).contains(name)) {
        *error = QCoreApplication::translate("MainWindow", "分类不存在。");
        return false;
    }
    return true;
}

bool available(const Notebook &book, const QString &name, QString *error) {
    if (name.isEmpty()) {
        *error = QCoreApplication::translate("MainWindow", "分类名称不能为空。");
        return false;
    }
    const bool reserved = book.type == "todo" && (name == QStringLiteral("全部") || name == QCoreApplication::translate("MainWindow", "全部") || name == QCoreApplication::translate("MainWindow", "默认"));
    if (reserved || categories(book).contains(name)) {
        *error = QCoreApplication::translate("MainWindow", "该分类已存在，请使用其他名称。");
        return false;
    }
    return true;
}
}// namespace

QStringList NotebookCategories::site(const Notebook &book) {
    return collect(book, QStringLiteral("default"));
}

QStringList NotebookCategories::todo(const Notebook &book) {
    auto names = collect(book, QStringLiteral("默认"));
    names.removeAll(QStringLiteral("全部"));
    return names;
}

bool NotebookCategories::create(Notebook &book, const QString &name, QString *error) {
    const auto title = name.trimmed();
    if (!available(book, title, error)) return false;
    auto names = book.config["categories"].toArray();
    names.append(title);
    book.config["categories"] = names;
    return true;
}

bool NotebookCategories::rename(Notebook &book, const QString &previous, const QString &name, QString *error) {
    if (!editable(book, previous, error)) return false;
    const auto title = name.trimmed();
    if (title == previous) return true;
    if (!available(book, title, error)) return false;
    auto names = categories(book);
    names.replace(names.indexOf(previous), title);
    book.config["categories"] = QJsonArray::fromStringList(names);
    for (auto &note: book.notes) {
        if (note.metadata["category"].toString() == previous) note.metadata["category"] = title;
    }
    return true;
}

bool NotebookCategories::remove(Notebook &book, const QString &name, QString *error) {
    if (!editable(book, name, error)) return false;
    auto names = categories(book);
    names.removeAll(name);
    book.config["categories"] = QJsonArray::fromStringList(names);
    for (auto &note: book.notes) {
        if (note.metadata["category"].toString() == name) note.metadata["category"] = fallback(book);
    }
    return true;
}
