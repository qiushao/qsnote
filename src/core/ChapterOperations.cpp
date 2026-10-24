#include "ChapterOperations.h"

namespace {
struct Position {
    int source = -1;
    int previous = -1;
    int next = -1;
    int parent = -1;
};

Position position(const Notebook &book, const QString &id) {
    Position result;
    if (book.type != "ebook") return result;
    for (int index = 0; index < book.notes.size(); ++index) {
        if (book.notes[index].id == id) {
            result.source = index;
            break;
        }
    }
    if (result.source < 0) return result;
    const auto &note = book.notes[result.source];
    for (int index = 0; index < book.notes.size(); ++index) {
        const auto &candidate = book.notes[index];
        if (candidate.id == note.parentId) result.parent = index;
        if (candidate.parentId != note.parentId) continue;
        if (index < result.source) result.previous = index;
        if (index > result.source && result.next < 0) result.next = index;
    }
    return result;
}

bool allowed(const Position &position, ChapterOperations::Move movement) {
    if (position.source < 0) return false;
    switch (movement) {
        case ChapterOperations::Move::Up:
        case ChapterOperations::Move::Indent:
            return position.previous >= 0;
        case ChapterOperations::Move::Down:
            return position.next >= 0;
        case ChapterOperations::Move::Outdent:
            return position.parent >= 0;
    }
    return false;
}
}// namespace

bool ChapterOperations::canMove(const Notebook &book, const QString &id, Move movement) {
    return allowed(position(book, id), movement);
}

bool ChapterOperations::move(Notebook &book, const QString &id, Move movement) {
    const auto location = position(book, id);
    if (!allowed(location, movement)) return false;
    switch (movement) {
        case Move::Up:
            book.notes.swapItemsAt(location.source, location.previous);
            break;
        case Move::Down:
            book.notes.swapItemsAt(location.source, location.next);
            break;
        case Move::Indent:
            book.notes[location.source].parentId = book.notes[location.previous].id;
            break;
        case Move::Outdent:
            book.notes[location.source].parentId = book.notes[location.parent].parentId;
            book.notes.move(location.source, location.source < location.parent ? location.parent : location.parent + 1);
            break;
    }
    return true;
}
