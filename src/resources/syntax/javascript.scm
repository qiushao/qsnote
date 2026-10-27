(class_declaration name: (_) @type)
(class name: (_) @type)
(new_expression constructor: (identifier) @type)
(property_identifier) @property
(private_property_identifier) @property
(shorthand_property_identifier) @property
(shorthand_property_identifier_pattern) @property
(function_declaration name: (identifier) @function)
(function_expression name: (identifier) @function)
(generator_function_declaration name: (identifier) @function)
(generator_function name: (identifier) @function)
(method_definition name: [(property_identifier) (private_property_identifier)] @function)
(call_expression function: (identifier) @function)
(call_expression function: (member_expression property: [(property_identifier) (private_property_identifier)] @function))
(variable_declarator name: (identifier) @function value: [(function_expression) (arrow_function)])
(pair key: (property_identifier) @function value: [(function_expression) (arrow_function)])
(assignment_expression left: (identifier) @function right: [(function_expression) (arrow_function)])
