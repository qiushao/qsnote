(type_identifier) @type
(class_declaration name: (identifier) @type)
(interface_declaration name: (identifier) @type)
(enum_declaration name: (identifier) @type)
(record_declaration name: (identifier) @type)
(field_declaration declarator: (variable_declarator name: (identifier) @property))
(field_access field: (identifier) @property)
(method_declaration name: (identifier) @function)
(method_invocation name: (identifier) @function)
(constructor_declaration name: (identifier) @function)
["record" "sealed" "permits" "yield"] @keyword
