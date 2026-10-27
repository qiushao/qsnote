#include "SourceOutline.h"
#include "SourceLanguage.h"
#include <QCoreApplication>
#include <QHash>
#include <QSet>
#include <cstring>
#include <memory>
#include <tree_sitter/api.h>

namespace {
using SourceOutline::Kind;
using SourceOutline::Symbol;

TSNode field(TSNode node, const char *name) {
    if (ts_node_is_null(node)) return {};
    return ts_node_child_by_field_name(node, name, static_cast<uint32_t>(std::strlen(name)));
}

bool isType(TSNode node, const char *type) {
    return !ts_node_is_null(node) && std::strcmp(ts_node_type(node), type) == 0;
}

// Declarator wrappers carry the declared name, not the identifiers in parameters or initializers.
TSNode declaredName(TSNode node) {
    while (!ts_node_is_null(node)) {
        if (isType(node, "qualified_identifier") || isType(node, "destructor_name") || isType(node, "operator_name")) return node;
        const auto declarator = field(node, "declarator");
        if (!ts_node_is_null(declarator)) {
            node = declarator;
            continue;
        }
        const auto name = field(node, "name");
        if (!ts_node_is_null(name)) return name;
        if (!isType(node, "parenthesized_declarator") && !isType(node, "reference_declarator")) return node;
        node = ts_node_named_child(node, 0);
    }
    return node;
}

bool isFunction(TSNode node) {
    while (!ts_node_is_null(node)) {
        if (isType(node, "function_declarator")) return !isType(field(node, "declarator"), "parenthesized_declarator");
        node = field(node, "declarator");
    }
    return false;
}

bool isCallableValue(TSNode node) {
    return isType(node, "arrow_function") || isType(node, "function_expression") || isType(node, "generator_function") || isType(node, "lambda");
}

class Collector {
public:
    explicit Collector(const QByteArray &source) : source_(source) {}

    QVector<Symbol> collect(TSNode root) {
        visit(root, -1);
        // Implementation files often include the class declaration from a header.
        // Group qualified definitions even when their owning class is absent here.
        QHash<QString, int> scopes;
        for (int index = 0; index < symbols_.size(); ++index) {
            const auto &symbol = symbols_[index];
            if (symbol.kind == Kind::Class || symbol.kind == Kind::Namespace) scopes.insert(qualifiedName(index), index);
        }
        const int symbolCount = static_cast<int>(symbols_.size());
        for (int index = 0; index < symbolCount; ++index) {
            auto symbol = symbols_[index];
            const auto separator = symbol.name.lastIndexOf("::");
            if (separator < 0) continue;
            const auto prefix = symbol.parent < 0 ? QString() : qualifiedName(symbol.parent) + "::";
            const auto scope = symbol.name.left(separator);
            int owner = scopes.value(prefix + scope, -1);
            if (owner < 0 && (symbol.kind == Kind::Function || symbol.kind == Kind::Variable)) {
                int parent = symbol.parent;
                auto name = scope;
                const auto scopeSeparator = scope.lastIndexOf("::");
                if (scopeSeparator >= 0) {
                    const int ancestor = scopes.value(prefix + scope.left(scopeSeparator), -1);
                    if (ancestor >= 0) {
                        parent = ancestor;
                        name = scope.mid(scopeSeparator + 2);
                    }
                }
                owner = static_cast<int>(symbols_.size());
                symbols_.append({name, Kind::Class, symbol.line, parent});
                scopes.insert(prefix + scope, owner);
            }
            if (owner < 0) continue;
            symbol.parent = owner;
            symbol.name = symbol.name.mid(separator + 2);
            symbol.kind = memberKind(symbol.kind, owner);
            symbols_[index] = symbol;
        }
        return symbols_;
    }

private:
    [[nodiscard]] QString text(TSNode node) const {
        if (ts_node_is_null(node)) return {};
        return QString::fromUtf8(source_.constData() + ts_node_start_byte(node), ts_node_end_byte(node) - ts_node_start_byte(node));
    }

    [[nodiscard]] QString qualifiedName(int index) const {
        QStringList names;
        while (index >= 0) {
            names.prepend(symbols_[index].name);
            index = symbols_[index].parent;
        }
        return names.join("::");
    }

    [[nodiscard]] Kind memberKind(Kind kind, int parent) const {
        if (parent >= 0 && (symbols_[parent].kind == Kind::Class || symbols_[parent].kind == Kind::Enum)) {
            if (kind == Kind::Variable) return Kind::Field;
            if (kind == Kind::Function) return Kind::Method;
        }
        return kind;
    }

    int add(TSNode name, Kind kind, int parent) {
        if (ts_node_is_null(name) || ts_node_is_missing(name) || text(name).isEmpty()) return parent;
        symbols_.append({text(name), memberKind(kind, parent), static_cast<int>(ts_node_start_point(name).row), parent});
        return static_cast<int>(symbols_.size()) - 1;
    }

    void bindings(TSNode node, Kind kind, int parent) {// NOLINT(misc-no-recursion): Walks nested binding patterns in the syntax tree.
        if (ts_node_is_null(node)) return;
        if (isType(node, "identifier") || isType(node, "shorthand_property_identifier_pattern")) {
            add(node, kind, parent);
        } else if (isType(node, "pair_pattern")) {
            bindings(field(node, "value"), kind, parent);
        } else if (isType(node, "assignment_pattern") || isType(node, "object_assignment_pattern")) {
            bindings(field(node, "left"), kind, parent);
        } else if (!isType(node, "attribute") && !isType(node, "subscript")) {
            for (uint32_t index = 0; index < ts_node_named_child_count(node); ++index) bindings(ts_node_named_child(node, index), kind, parent);
        }
    }

    void instanceFields(TSNode node, int parent) {// NOLINT(misc-no-recursion): Walks method syntax trees, excluding nested functions and classes.
        if (ts_node_is_null(node) || isType(node, "function_definition") || isType(node, "class_definition") || isType(node, "function_declaration") || isType(node, "function_expression") || isType(node, "arrow_function") || isType(node, "class")) return;
        if (isType(node, "assignment") || isType(node, "assignment_expression")) {
            const auto left = field(node, "left");
            const auto object = text(field(left, "object"));
            if ((isType(left, "attribute") && (object == "self" || object == "cls")) || (isType(left, "member_expression") && object == "this")) {
                const auto name = field(left, isType(left, "attribute") ? "attribute" : "property");
                bool exists = false;
                for (const auto &symbol: symbols_) {
                    if (symbol.parent == parent && symbol.kind == Kind::Field && symbol.name == text(name)) exists = true;
                }
                if (!exists) add(name, Kind::Field, parent);
            }
        }
        for (uint32_t index = 0; index < ts_node_named_child_count(node); ++index) instanceFields(ts_node_named_child(node, index), parent);
    }

    void visit(TSNode node, int parent) {// NOLINT(misc-no-recursion): Symbol ownership follows the syntax tree hierarchy.
        if (ts_node_is_null(node) || ts_node_is_error(node)) return;
        const QByteArray type(ts_node_type(node));
        static const QSet<QByteArray> classes = {"class_specifier", "struct_specifier", "union_specifier", "class_declaration", "class_definition", "class", "interface_declaration", "record_declaration", "annotation_type_declaration"};
        static const QSet<QByteArray> functions = {"function_definition", "function_declaration", "generator_function_declaration", "method_definition", "method_declaration", "constructor_declaration", "compact_constructor_declaration", "function_signature", "method_signature", "abstract_method_signature", "annotation_type_element_declaration"};
        if (classes.contains(type) || type == "enum_specifier" || type == "enum_declaration" || type == "namespace_definition" || type == "internal_module") {
            const Kind kind = type.startsWith("enum_") ? Kind::Enum : type == "namespace_definition" || type == "internal_module" ? Kind::Namespace
                                                                                                                                  : Kind::Class;
            const int owner = add(field(node, "name"), kind, parent);
            if (type == "record_declaration") {
                const auto parameters = field(node, "parameters");
                for (uint32_t index = 0; index < ts_node_named_child_count(parameters); ++index) add(field(ts_node_named_child(parameters, index), "name"), Kind::Field, owner);
            }
            visit(field(node, "body"), owner);
            return;
        }
        if (functions.contains(type)) {
            auto name = field(node, "name");
            if (ts_node_is_null(name)) name = declaredName(field(node, "declarator"));
            add(name, Kind::Function, parent);
            if ((type == "function_definition" || type == "method_definition") && parent >= 0 && symbols_[parent].kind == Kind::Class) instanceFields(field(node, "body"), parent);
            return;
        }
        if (type == "type_definition") {
            const auto target = field(node, "type");
            const auto name = declaredName(field(node, "declarator"));
            if (ts_node_is_null(field(target, "name")) && !ts_node_is_null(field(target, "body"))) {
                const int owner = add(name, isType(target, "enum_specifier") ? Kind::Enum : Kind::Class, parent);
                visit(field(target, "body"), owner);
            } else {
                visit(target, parent);
                add(name, Kind::Type, parent);
            }
            return;
        }
        if (type == "declaration" || type == "field_declaration") {
            visit(field(node, "type"), parent);
            for (uint32_t index = 0; index < ts_node_child_count(node); ++index) {
                const auto *role = ts_node_field_name_for_child(node, index);
                if (!role || std::strcmp(role, "declarator") != 0) continue;
                const auto child = ts_node_child(node, index);
                add(declaredName(child), isFunction(child) ? Kind::Function : Kind::Variable, parent);
            }
            return;
        }
        if (type == "variable_declarator") {
            const auto name = field(node, "name");
            const auto value = field(node, "value");
            if (isType(value, "class")) {
                const int owner = add(name, Kind::Class, parent);
                visit(field(value, "body"), owner);
            } else {
                bindings(name, isCallableValue(value) ? Kind::Function : Kind::Variable, parent);
            }
            return;
        }
        if (type == "assignment") {
            const auto value = field(node, "right");
            bindings(field(node, "left"), isCallableValue(value) ? Kind::Function : Kind::Variable, parent);
            if (isType(value, "assignment")) visit(value, parent);
            return;
        }
        if (type == "field_definition" || type == "public_field_definition" || type == "property_signature") {
            auto name = field(node, "name");
            if (ts_node_is_null(name)) name = field(node, "property");
            add(name, isCallableValue(field(node, "value")) ? Kind::Function : Kind::Variable, parent);
            return;
        }
        if (type == "enumerator" || type == "enum_constant" || type == "enum_assignment" || type == "variable_assignment" || type == "alias_declaration" || type == "type_alias_declaration") {
            add(field(node, "name"), type == "alias_declaration" || type == "type_alias_declaration" ? Kind::Type : Kind::Variable, parent);
            return;
        }
        if (type == "enum_body") {
            for (uint32_t index = 0; index < ts_node_named_child_count(node); ++index) {
                const auto child = ts_node_named_child(node, index);
                if (isType(child, "property_identifier")) add(child, Kind::Field, parent);
                else
                    visit(child, parent);
            }
            return;
        }
        if (type == "static_initializer" || type == "class_static_block" || type == "lambda_expression" || type == "lambda" || type == "arrow_function" || type == "function_expression" || type == "generator_function" || type == "command") return;
        for (uint32_t index = 0; index < ts_node_named_child_count(node); ++index) visit(ts_node_named_child(node, index), parent);
    }

    const QByteArray &source_;
    QVector<Symbol> symbols_;
};
}// namespace

SourceOutline::Result SourceOutline::parse(const QString &fileName, const QString &source) {
    const auto *language = SourceLanguage::forFile(fileName).grammar;
    if (!language) return {};
    const std::unique_ptr<TSParser, decltype(&ts_parser_delete)> parser(ts_parser_new(), ts_parser_delete);
    if (!ts_parser_set_language(parser.get(), language)) return {};
    const auto bytes = source.toUtf8();
    const std::unique_ptr<TSTree, decltype(&ts_tree_delete)> tree(ts_parser_parse_string(parser.get(), nullptr, bytes.constData(), static_cast<uint32_t>(bytes.size())), ts_tree_delete);
    return {true, tree ? Collector(bytes).collect(ts_tree_root_node(tree.get())) : QVector<Symbol>()};
}

QString SourceOutline::kindName(Kind kind) {
    switch (kind) {
        case Kind::Namespace:
            return QCoreApplication::translate("SourceOutline", "命名空间");
        case Kind::Class:
            return QCoreApplication::translate("SourceOutline", "类");
        case Kind::Enum:
            return QCoreApplication::translate("SourceOutline", "枚举");
        case Kind::Function:
            return QCoreApplication::translate("SourceOutline", "函数");
        case Kind::Method:
            return QCoreApplication::translate("SourceOutline", "成员函数");
        case Kind::Variable:
            return QCoreApplication::translate("SourceOutline", "全局变量");
        case Kind::Field:
            return QCoreApplication::translate("SourceOutline", "成员变量");
        case Kind::Type:
            return QCoreApplication::translate("SourceOutline", "类型");
    }
    return {};
}
