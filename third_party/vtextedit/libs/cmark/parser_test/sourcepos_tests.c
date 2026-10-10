// Source-position bookkeeping must be performed during parsing regardless of
// CMARK_OPT_SOURCEPOS -- that option only controls whether a *renderer* emits
// the positions (see the contract in cmark.h).
//
// Before this was fixed, `adjust_subj_node_newlines()` returned early without
// CMARK_OPT_SOURCEPOS, so after a newline-crossing code span or a newline-
// crossing inline HTML tag every *subsequent* inline node in the paragraph had
// stale coordinates when parsed with CMARK_OPT_DEFAULT.
//
// The tests below parse the same document twice -- once with CMARK_OPT_DEFAULT
// and once with CMARK_OPT_SOURCEPOS -- and require every node's recorded
// position to be identical.

#include <stdio.h>
#include <string.h>

#include <cmark.h>

#include "test_utils.h"

// Walk both trees in parallel and compare every node's recorded position.
static int compare_positions(const char *markdown) {
  cmark_node *plain =
      cmark_parse_document(markdown, strlen(markdown), CMARK_OPT_DEFAULT);
  cmark_node *posed =
      cmark_parse_document(markdown, strlen(markdown), CMARK_OPT_SOURCEPOS);

  cmark_iter *plain_iter = cmark_iter_new(plain);
  cmark_iter *posed_iter = cmark_iter_new(posed);

  int ret = 1;
  while (1) {
    cmark_event_type ev_a = cmark_iter_next(plain_iter);
    cmark_event_type ev_b = cmark_iter_next(posed_iter);
    if (ev_a != ev_b) {
      fprintf(stderr, "Tree shape differs for input:\n%s\n", markdown);
      ret = 0;
      break;
    }
    if (ev_a == CMARK_EVENT_DONE) {
      break;
    }
    if (ev_a != CMARK_EVENT_ENTER) {
      continue;
    }

    cmark_node *a = cmark_iter_get_node(plain_iter);
    cmark_node *b = cmark_iter_get_node(posed_iter);
    if (cmark_node_get_type(a) != cmark_node_get_type(b)) {
      fprintf(stderr, "Node type differs for input:\n%s\n", markdown);
      ret = 0;
      break;
    }
    if (cmark_node_get_start_line(a) != cmark_node_get_start_line(b) ||
        cmark_node_get_start_column(a) != cmark_node_get_start_column(b) ||
        cmark_node_get_end_line(a) != cmark_node_get_end_line(b) ||
        cmark_node_get_end_column(a) != cmark_node_get_end_column(b)) {
      fprintf(stderr,
              "Position differs for <%s> in input:\n%s\n"
              "  CMARK_OPT_DEFAULT:   %d:%d-%d:%d\n"
              "  CMARK_OPT_SOURCEPOS: %d:%d-%d:%d\n",
              cmark_node_get_type_string(a), markdown,
              cmark_node_get_start_line(a), cmark_node_get_start_column(a),
              cmark_node_get_end_line(a), cmark_node_get_end_column(a),
              cmark_node_get_start_line(b), cmark_node_get_start_column(b),
              cmark_node_get_end_line(b), cmark_node_get_end_column(b));
      ret = 0;
      break;
    }
  }

  cmark_iter_free(plain_iter);
  cmark_iter_free(posed_iter);
  cmark_node_free(plain);
  cmark_node_free(posed);
  return ret;
}

// Locate the first node of `type` and check its recorded span, parsing WITHOUT
// CMARK_OPT_SOURCEPOS. Guards against both parses being wrong in the same way.
static int check_first_node(const char *markdown, cmark_node_type type,
                            int start_line, int start_column, int end_line,
                            int end_column) {
  cmark_node *doc =
      cmark_parse_document(markdown, strlen(markdown), CMARK_OPT_DEFAULT);
  cmark_iter *iter = cmark_iter_new(doc);

  int ret = 0;
  cmark_event_type ev;
  while ((ev = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
    if (ev != CMARK_EVENT_ENTER) {
      continue;
    }
    cmark_node *node = cmark_iter_get_node(iter);
    if (cmark_node_get_type(node) != type) {
      continue;
    }

    ret = cmark_node_get_start_line(node) == start_line &&
          cmark_node_get_start_column(node) == start_column &&
          cmark_node_get_end_line(node) == end_line &&
          cmark_node_get_end_column(node) == end_column;
    if (!ret) {
      fprintf(stderr,
              "Unexpected span for <%s> in input:\n%s\n"
              "  expected: %d:%d-%d:%d\n"
              "  actual:   %d:%d-%d:%d\n",
              cmark_node_get_type_string(node), markdown, start_line,
              start_column, end_line, end_column,
              cmark_node_get_start_line(node),
              cmark_node_get_start_column(node),
              cmark_node_get_end_line(node), cmark_node_get_end_column(node));
    }
    break;
  }

  if (!ret && ev == CMARK_EVENT_DONE) {
    fprintf(stderr, "No node of the requested type in input:\n%s\n", markdown);
  }

  cmark_iter_free(iter);
  cmark_node_free(doc);
  return ret;
}

int test_after_multiline_code_span() {
  return compare_positions("a `co\nde` <span>b</span> c\n");
}

int test_after_multiline_inline_html() {
  return compare_positions("a <span\nclass=\"x\">b</span> `code` d\n");
}

int test_after_multiline_code_span_in_block_quote() {
  return compare_positions("> a `co\n> de` <span>b</span> c\n");
}

int test_after_multiline_inline_html_in_block_quote() {
  return compare_positions("> a <span\n> class=\"x\">b</span> `code` d\n");
}

int test_after_multiline_code_span_in_list_item() {
  return compare_positions("- a `co\n  de` <span>b</span> c\n");
}

int test_after_multiline_inline_html_in_list_item() {
  return compare_positions("- a <span\n  class=\"x\">b</span> `code` d\n");
}

int test_multiline_constructs_chained() {
  return compare_positions("a `co\nde` b <span\nclass=\"x\">c</span> `d` e\n");
}

// Non-regression: a wrapped link destination is handled separately by an
// unconditional subj_catch_up_newlines() and was already correct.
int test_wrapped_link_destination() {
  return compare_positions("[t](\nfoo\n) `code` <span>x</span>\n");
}

// The HTML tag on line 2 starts at column 5 (after "de` "), not at a column
// carried over from line 1.
int test_absolute_span_after_multiline_code_span() {
  return check_first_node("a `co\nde` <span>b</span> c\n",
                          CMARK_NODE_HTML_INLINE, 2, 5, 2, 10);
}

// The code span on line 2 starts after `class="x">b</span> `.
int test_absolute_span_after_multiline_inline_html() {
  return check_first_node("a <span\nclass=\"x\">b</span> `code` d\n",
                          CMARK_NODE_CODE, 2, 20, 2, 25);
}

// A multiline construct's OWN end column must cover its closing delimiter.
//
// adjust_subj_node_newlines() derives it from a scan that stops BEFORE the
// closing delimiter (the backtick run / the `>`), so the delimiter's width has
// to be added back, along with block_offset -- the container prefix width that
// every column this parser reports includes. Omitting them ended a code span
// `1 + prefix width` characters early, which cut off the closing backtick.
int test_multiline_code_span_own_span() {
  // Line 2 is "de` b": the closing backtick is column 3.
  return check_first_node("a `co\nde` b\n", CMARK_NODE_CODE, 1, 3, 2, 3);
}

int test_multiline_code_span_own_span_in_block_quote() {
  // Line 2 is "> de` b": the closing backtick is column 5.
  return check_first_node("> a `co\n> de` b\n", CMARK_NODE_CODE, 1, 5, 2, 5);
}

int test_multiline_code_span_own_span_in_list_item() {
  // Line 2 is "  de` b": the closing backtick is column 5.
  return check_first_node("- a `co\n  de` b\n", CMARK_NODE_CODE, 1, 5, 2, 5);
}

// A multi-character delimiter: the whole closing run is part of the span.
int test_multiline_code_span_own_span_long_delimiter() {
  // Line 2 is "  de`` b": the closing run ends at column 6.
  return check_first_node("- a ``co\n  de`` b\n", CMARK_NODE_CODE, 1, 5, 2, 6);
}

// A lazy continuation line carries no container prefix, but the reported column
// still includes block_offset -- the same convention subj_line_column_at()
// applies, and what an ancestor paragraph reports for the same position. This
// pins the convention rather than the source column (which is 3 here).
int test_multiline_code_span_own_span_lazy_continuation() {
  return check_first_node("- a `co\nde` b\n", CMARK_NODE_CODE, 1, 5, 2, 5);
}

int test_multiline_inline_html_own_span() {
  // Line 2 is "class=\"x\">b</span> c": the tag's `>` is column 10.
  return check_first_node("a <span\nclass=\"x\">b</span> c\n",
                          CMARK_NODE_HTML_INLINE, 1, 3, 2, 10);
}

int main() {
  CASE(test_after_multiline_code_span);
  CASE(test_after_multiline_inline_html);
  CASE(test_after_multiline_code_span_in_block_quote);
  CASE(test_after_multiline_inline_html_in_block_quote);
  CASE(test_after_multiline_code_span_in_list_item);
  CASE(test_after_multiline_inline_html_in_list_item);
  CASE(test_multiline_constructs_chained);
  CASE(test_wrapped_link_destination);
  CASE(test_absolute_span_after_multiline_code_span);
  CASE(test_absolute_span_after_multiline_inline_html);
  CASE(test_multiline_code_span_own_span);
  CASE(test_multiline_code_span_own_span_in_block_quote);
  CASE(test_multiline_code_span_own_span_in_list_item);
  CASE(test_multiline_code_span_own_span_long_delimiter);
  CASE(test_multiline_code_span_own_span_lazy_continuation);
  CASE(test_multiline_inline_html_own_span);
  return 0;
}
