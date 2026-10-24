#pragma once

#include <QString>

// SVG images cannot fetch external fonts. Returns empty with an error on resource failure.
QString embeddedMathStylesheet(QString *error);
