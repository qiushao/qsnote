#include "SourceLanguage.h"
#include <QFileInfo>
#include <QHash>

extern "C" {
// NOLINTBEGIN(readability-identifier-naming): Upstream grammar entry points have a fixed C ABI.
const TSLanguage *tree_sitter_c();
const TSLanguage *tree_sitter_cpp();
const TSLanguage *tree_sitter_java();
const TSLanguage *tree_sitter_python();
const TSLanguage *tree_sitter_javascript();
const TSLanguage *tree_sitter_typescript();
const TSLanguage *tree_sitter_tsx();
const TSLanguage *tree_sitter_bash();
// NOLINTEND(readability-identifier-naming)
}


SourceLanguage::Definition SourceLanguage::forFile(const QString &fileName) {
    static const QHash<QString, QString> extensions = {
            {"c", "c"},
            {"cpp", "cpp"},
            {"cc", "cpp"},
            {"cxx", "cpp"},
            {"c++", "cpp"},
            {"h", "cpp"},
            {"hpp", "cpp"},
            {"hh", "cpp"},
            {"hxx", "cpp"},
            {"ipp", "cpp"},
            {"inl", "cpp"},
            {"java", "java"},
            {"py", "python"},
            {"pyw", "python"},
            {"pyi", "python"},
            {"js", "javascript"},
            {"jsx", "javascript"},
            {"mjs", "javascript"},
            {"cjs", "javascript"},
            {"ts", "typescript"},
            {"mts", "typescript"},
            {"cts", "typescript"},
            {"tsx", "tsx"},
            {"sh", "bash"},
            {"bash", "bash"}};
    using Factory = const TSLanguage *(*) ();
    static const QHash<QString, Factory> grammars = {
            {"c", tree_sitter_c},
            {"cpp", tree_sitter_cpp},
            {"java", tree_sitter_java},
            {"python", tree_sitter_python},
            {"javascript", tree_sitter_javascript},
            {"typescript", tree_sitter_typescript},
            {"tsx", tree_sitter_tsx},
            {"bash", tree_sitter_bash}};
    const QFileInfo info(fileName);
    auto name = extensions.value(info.suffix().toLower());
    if (info.suffix() == "C") name = "cpp";
    if (info.fileName() == ".bashrc" || info.fileName() == ".bash_profile" || info.fileName() == ".profile") name = "bash";
    const auto factory = grammars.value(name, nullptr);
    return {factory ? factory() : nullptr, name};
}
