#include "NotebookCategories.h"
#include "SiteDocuments.h"
#include <QCoreApplication>
#include <QJsonArray>
#include <QRegularExpression>

namespace {
QStringList collect(const Notebook &book, const QString &fallback) {
    QStringList names;
    for (const auto &value: book.config["categories"].toArray()) names << value.toString();
    if (!names.contains(fallback)) names.prepend(fallback);
    for (const auto &note: book.notes) if (!SiteDocuments::isDocument(note)) names << note.metadata["category"].toString();
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

QString NotebookCategories::slug(const Notebook &book, const QString &name) {
    const auto category = name.isEmpty() ? QStringLiteral("default") : name;
    return book.config["categorySlugs"].toObject()[category].toString(category);
}

bool NotebookCategories::setSlug(Notebook &book, const QString &name, const QString &value, QString *error) {
    const auto slug = value.trimmed();
    static const QRegularExpression valid(QStringLiteral("^[\\p{L}\\p{N}_-]+$"));
    if (!valid.match(slug).hasMatch()) {
        *error = QCoreApplication::translate("MainWindow", "分类 Slug 只能包含字母、数字、下划线和连字符。");
        return false;
    }
    for (const auto &category: site(book)) {
        if (category != name && NotebookCategories::slug(book, category) == slug) {
            *error = QCoreApplication::translate("MainWindow", "分类 Slug 已存在。");
            return false;
        }
    }
    auto slugs = book.config["categorySlugs"].toObject();
    slugs[name] = slug;
    book.config["categorySlugs"] = slugs;
    return true;
}

QStringList NotebookCategories::todo(const Notebook &book) {
    auto names = collect(book, QStringLiteral("默认"));
    names.removeAll(QStringLiteral("默认"));
    names.prepend(QStringLiteral("默认"));
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
    if (book.type == "site") {
        auto slugs = book.config["categorySlugs"].toObject();
        slugs[title] = slug(book, previous);
        slugs.remove(previous);
        book.config["categorySlugs"] = slugs;
    }
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
    if (book.type == "site") {
        auto slugs = book.config["categorySlugs"].toObject();
        slugs.remove(name);
        book.config["categorySlugs"] = slugs;
    }
    book.config["categories"] = QJsonArray::fromStringList(names);
    for (auto &note: book.notes) {
        if (note.metadata["category"].toString() == name) note.metadata["category"] = fallback(book);
    }
    return true;
}
