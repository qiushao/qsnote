#include "cmark.h"
#include "utf8.h"
#include "parser.h"
#include "footnotes.h"
#include "chunk.h"

static void footnote_free(cmark_footnote_map *map, cmark_footnote *fn) {
  cmark_mem *mem = map->mem;
  if (fn != NULL) {
    mem->free(fn->label);
    mem->free(fn);
  }
}

// normalize footnote label:  collapse internal whitespace to single space,
// remove leading/trailing whitespace, case fold
// Return NULL if the label is actually empty (i.e. composed
// solely from whitespace)
static unsigned char *normalize_footnote_label(cmark_mem *mem,
                                               cmark_chunk *ref) {
  cmark_strbuf normalized = CMARK_BUF_INIT(mem);
  unsigned char *result;

  if (ref == NULL)
    return NULL;

  if (ref->len == 0)
    return NULL;

  cmark_utf8proc_case_fold(&normalized, ref->data, ref->len);
  cmark_strbuf_trim(&normalized);
  cmark_strbuf_normalize_whitespace(&normalized);

  result = cmark_strbuf_detach(&normalized);
  assert(result);

  if (result[0] == '\0') {
    mem->free(result);
    return NULL;
  }

  return result;
}

void cmark_footnote_create(cmark_footnote_map *map, cmark_chunk *label,
                           struct cmark_node *def_node) {
  cmark_footnote *fn;
  unsigned char *norm = normalize_footnote_label(map->mem, label);

  /* empty label, or composed from only whitespace */
  if (norm == NULL)
    return;

  assert(map->sorted == NULL);

  fn = (cmark_footnote *)map->mem->calloc(1, sizeof(*fn));
  fn->label = norm;
  fn->def_node = def_node;
  fn->number = 0;
  fn->age = map->size;
  fn->next = map->footnotes;

  map->footnotes = fn;
  map->size++;
}

static int labelcmp(const unsigned char *a, const unsigned char *b) {
  return strcmp((const char *)a, (const char *)b);
}

static int footnotecmp(const void *p1, const void *p2) {
  cmark_footnote *f1 = *(cmark_footnote **)p1;
  cmark_footnote *f2 = *(cmark_footnote **)p2;
  int res = labelcmp(f1->label, f2->label);
  return res ? res : ((int)f1->age - (int)f2->age);
}

static int footnotesearch(const void *label, const void *p2) {
  cmark_footnote *fn = *(cmark_footnote **)p2;
  return labelcmp((const unsigned char *)label, fn->label);
}

static void sort_footnotes(cmark_footnote_map *map) {
  unsigned int i = 0, last = 0, size = map->size;
  cmark_footnote *f = map->footnotes, **sorted = NULL;

  sorted =
      (cmark_footnote **)map->mem->calloc(size, sizeof(cmark_footnote *));
  while (f) {
    sorted[i++] = f;
    f = f->next;
  }

  qsort(sorted, size, sizeof(cmark_footnote *), footnotecmp);

  for (i = 1; i < size; i++) {
    if (labelcmp(sorted[i]->label, sorted[last]->label) != 0)
      sorted[++last] = sorted[i];
  }
  map->sorted = sorted;
  map->size = last + 1;
}

// Returns footnote if map contains a footnote with matching
// label, otherwise NULL.
cmark_footnote *cmark_footnote_lookup(cmark_footnote_map *map,
                                      cmark_chunk *label) {
  cmark_footnote **fn = NULL;
  unsigned char *norm;

  if (label->len < 1 || label->len > MAX_LINK_LABEL_LENGTH)
    return NULL;

  if (map == NULL || !map->size)
    return NULL;

  norm = normalize_footnote_label(map->mem, label);
  if (norm == NULL)
    return NULL;

  if (!map->sorted)
    sort_footnotes(map);

  fn = (cmark_footnote **)bsearch(norm, map->sorted, map->size,
                                  sizeof(cmark_footnote *), footnotesearch);
  map->mem->free(norm);

  if (fn != NULL)
    return fn[0];

  return NULL;
}

void cmark_footnote_map_free(cmark_footnote_map *map) {
  cmark_footnote *fn;

  if (map == NULL)
    return;

  fn = map->footnotes;
  while (fn) {
    cmark_footnote *next = fn->next;
    footnote_free(map, fn);
    fn = next;
  }

  map->mem->free(map->sorted);
  map->mem->free(map);
}

cmark_footnote_map *cmark_footnote_map_new(cmark_mem *mem) {
  cmark_footnote_map *map =
      (cmark_footnote_map *)mem->calloc(1, sizeof(cmark_footnote_map));
  map->mem = mem;
  map->next_number = 1;
  return map;
}
