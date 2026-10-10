#ifndef CMARK_AST_H
#define CMARK_AST_H

#include <stdio.h>
#include "references.h"
#include "footnotes.h"
#include "node.h"
#include "buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_LINK_LABEL_LENGTH 1000

struct cmark_parser {
  struct cmark_mem *mem;
  struct cmark_reference_map *refmap;
  struct cmark_footnote_map *footnote_map;
  struct cmark_node *root;
  struct cmark_node *current;
  int line_number;
  bufsize_t offset;
  bufsize_t column;
  bufsize_t first_nonspace;
  bufsize_t first_nonspace_column;
  bufsize_t thematic_break_kill_pos;
  int indent;
  bool blank;
  bool partially_consumed_tab;
  cmark_strbuf curline;
  bufsize_t last_line_length;
  cmark_strbuf linebuf;
  cmark_strbuf content;
  int content_start_line;
  // Final appended line and the preceding source line's end, for table interruption.
  bufsize_t content_last_line_offset;
  bufsize_t content_last_line_column;
  bufsize_t content_previous_line_length;
  cmark_reference_destination_callback reference_destination_callback;
  void *reference_destination_user_data;
  int options;
  bool last_buffer_ended_with_cr;
  unsigned int total_size;
};

#ifdef __cplusplus
}
#endif

#endif
