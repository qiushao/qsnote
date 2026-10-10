#ifndef CMARK_INLINES_H
#define CMARK_INLINES_H

#include "chunk.h"
#include "references.h"
#include "footnotes.h"

#ifdef __cplusplus
extern "C" {
#endif

unsigned char *cmark_clean_url(cmark_mem *mem, cmark_chunk *url);
unsigned char *cmark_clean_title(cmark_mem *mem, cmark_chunk *title);

void cmark_parse_inlines(cmark_mem *mem, cmark_node *parent,
                         cmark_reference_map *refmap,
                         cmark_footnote_map *footnote_map, int options);

bufsize_t cmark_parse_reference_inline(cmark_mem *mem, cmark_chunk *input,
                                       cmark_reference_map *refmap,
                                       bufsize_t *url_start, bufsize_t *url_end);

#ifdef __cplusplus
}
#endif

#endif
