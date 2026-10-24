#pragma once

#include "NotebookStore.h"

namespace ChapterOperations {
enum class Move { Up,
                  Down,
                  Indent,
                  Outdent };
bool canMove(const Notebook &book, const QString &id, Move movement);
// Changes ordering/parent metadata; callers save through the usual notebook path migration.
bool move(Notebook &book, const QString &id, Move movement);
}// namespace ChapterOperations
