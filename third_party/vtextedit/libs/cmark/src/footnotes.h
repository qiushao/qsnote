#ifndef CMARK_FOOTNOTES_H
#define CMARK_FOOTNOTES_H

#include "chunk.h"

#ifdef __cplusplus
extern "C" {
#endif

struct cmark_footnote {
  struct cmark_footnote *next;
  unsigned char *label;       // normalized (case-folded) label
  struct cmark_node *def_node; // pointer to FOOTNOTE_DEFINITION node
  int number;                 // 0 = unreferenced, >0 = assigned number
  unsigned int age;           // insertion order for sort stability
};

typedef struct cmark_footnote cmark_footnote;

struct cmark_footnote_map {
  cmark_mem *mem;
  cmark_footnote *footnotes;   // linked list head
  cmark_footnote **sorted;     // lazily built sorted array for bsearch
  unsigned int size;           // number of entries
  int next_number;             // next number to assign (starts at 1)
};

typedef struct cmark_footnote_map cmark_footnote_map;

cmark_footnote_map *cmark_footnote_map_new(cmark_mem *mem);
void cmark_footnote_map_free(cmark_footnote_map *map);
void cmark_footnote_create(cmark_footnote_map *map, cmark_chunk *label,
                           struct cmark_node *def_node);
cmark_footnote *cmark_footnote_lookup(cmark_footnote_map *map,
                                      cmark_chunk *label);

#ifdef __cplusplus
}
#endif

#endif
