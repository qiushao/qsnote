(namespace_identifier) @type
(call_expression function: (qualified_identifier name: (identifier) @function))
(function_declarator declarator: (qualified_identifier name: (identifier) @function))
(function_declarator declarator: (field_identifier) @function)
(function_declarator declarator: (destructor_name (identifier) @function))
(template_function name: (identifier) @function)
(template_method name: (field_identifier) @function)
["co_await" "co_return" "co_yield" "concept" "requires" "consteval" "constinit"] @keyword
