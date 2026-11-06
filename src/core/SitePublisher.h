#ifndef QSNOTE_SITEPUBLISHER_H
#define QSNOTE_SITEPUBLISHER_H

#include "NotebookStore.h"
#include <functional>

class SitePublisher {
public:
    // Completed steps, total steps, and the current operation (including command progress).
    using ProgressCallback = std::function<void(int, int, const QString &)>;
    static bool validate(const Notebook &book, QString *error);
    // Publish an already generated public directory.
    static bool publish(const Notebook &book, QString *error, const ProgressCallback &progress = {});
};

#endif
