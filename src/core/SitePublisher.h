#ifndef QSNOTE_SITEPUBLISHER_H
#define QSNOTE_SITEPUBLISHER_H

#include "NotebookStore.h"

class SitePublisher {
public:
    static bool validate(const Notebook &book, QString *error);
    // Publish an already generated public directory.
    static bool publish(const Notebook &book, QString *error);
};

#endif
