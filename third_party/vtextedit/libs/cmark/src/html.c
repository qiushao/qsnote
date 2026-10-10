#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cmark_ctype.h"
#include "cmark.h"
#include "node.h"
#include "buffer.h"
#include "houdini.h"
#include "scanners.h"

#define BUFFER_SIZE 100

// Functions to convert cmark_nodes to HTML strings.

static void escape_html(cmark_strbuf *dest, const unsigned char *source,
                        bufsize_t length) {
  houdini_escape_html(dest, source, length, 0);
}

static void escape_inline_formula(cmark_strbuf *dest,
                                  const unsigned char *source,
                                  bufsize_t length) {
  bufsize_t i, start = 0;

  if (length == 0)
    return;

  for (i = 0; i < length; ++i) {
    if (source[i] == '$') {
      escape_html(dest, source + start, i - start);
      // Restore the backslash removed by handle_dollar().
      cmark_strbuf_puts(dest, "\\$");
      start = i + 1;
    }
  }
  escape_html(dest, source + start, length - start);
}

static inline void cr(cmark_strbuf *html) {
  if (html->size && html->ptr[html->size - 1] != '\n')
    cmark_strbuf_putc(html, '\n');
}

struct render_state {
  cmark_strbuf *html;
  cmark_node *plain;
  cmark_strbuf *footnotes_html;
  bool in_footnote;
};

static void S_render_sourcepos(cmark_node *node, cmark_strbuf *html,
                               int options) {
  char buffer[BUFFER_SIZE];
  if (CMARK_OPT_SOURCEPOS & options) {
    snprintf(buffer, BUFFER_SIZE, " data-sourcepos=\"%d:%d-%d:%d\"",
             cmark_node_get_start_line(node), cmark_node_get_start_column(node),
             cmark_node_get_end_line(node), cmark_node_get_end_column(node));
    cmark_strbuf_puts(html, buffer);
  }
}

static int S_render_node(cmark_node *node, cmark_event_type ev_type,
                         struct render_state *state, int options) {
  cmark_node *parent;
  cmark_node *grandparent;
  cmark_strbuf *html = state->in_footnote ? state->footnotes_html : state->html;
  char start_heading[] = "<h0";
  char end_heading[] = "</h0";
  bool tight;
  char buffer[BUFFER_SIZE];

  bool entering = (ev_type == CMARK_EVENT_ENTER);

  if (state->plain == node) { // back at original node
    state->plain = NULL;
  }

  if (state->plain != NULL) {
    switch (node->type) {
    case CMARK_NODE_TEXT:
    case CMARK_NODE_CODE:
    case CMARK_NODE_HTML_INLINE:
      escape_html(html, node->data, node->len);
      break;

    case CMARK_NODE_LINEBREAK:
    case CMARK_NODE_SOFTBREAK:
      cmark_strbuf_putc(html, ' ');
      break;

    default:
      break;
    }
    return 1;
  }

  switch (node->type) {
  case CMARK_NODE_DOCUMENT:
    break;

  case CMARK_NODE_BLOCK_QUOTE:
    if (entering) {
      cr(html);
      cmark_strbuf_puts(html, "<blockquote");
      S_render_sourcepos(node, html, options);
      cmark_strbuf_puts(html, ">\n");
    } else {
      cr(html);
      cmark_strbuf_puts(html, "</blockquote>\n");
    }
    break;

  case CMARK_NODE_LIST: {
    cmark_list_type list_type = (cmark_list_type)node->as.list.list_type;
    int start = node->as.list.start;

    if (entering) {
      cr(html);
      if (list_type == CMARK_BULLET_LIST) {
        cmark_strbuf_puts(html, "<ul");
        S_render_sourcepos(node, html, options);
        cmark_strbuf_puts(html, ">\n");
      } else if (start == 1) {
        cmark_strbuf_puts(html, "<ol");
        S_render_sourcepos(node, html, options);
        cmark_strbuf_puts(html, ">\n");
      } else {
        snprintf(buffer, BUFFER_SIZE, "<ol start=\"%d\"", start);
        cmark_strbuf_puts(html, buffer);
        S_render_sourcepos(node, html, options);
        cmark_strbuf_puts(html, ">\n");
      }
    } else {
      cmark_strbuf_puts(html,
                        list_type == CMARK_BULLET_LIST ? "</ul>\n" : "</ol>\n");
    }
    break;
  }

  case CMARK_NODE_ITEM:
    if (entering) {
      cr(html);
      cmark_strbuf_puts(html, "<li");
      S_render_sourcepos(node, html, options);
      cmark_strbuf_putc(html, '>');
    } else {
      cmark_strbuf_puts(html, "</li>\n");
    }
    break;

  case CMARK_NODE_HEADING:
    if (entering) {
      cr(html);
      start_heading[2] = (char)('0' + node->as.heading.level);
      cmark_strbuf_puts(html, start_heading);
      S_render_sourcepos(node, html, options);
      cmark_strbuf_putc(html, '>');
    } else {
      end_heading[3] = (char)('0' + node->as.heading.level);
      cmark_strbuf_puts(html, end_heading);
      cmark_strbuf_puts(html, ">\n");
    }
    break;

  case CMARK_NODE_CODE_BLOCK:
    cr(html);

    if (node->as.code.info == NULL || node->as.code.info[0] == 0) {
      cmark_strbuf_puts(html, "<pre");
      S_render_sourcepos(node, html, options);
      cmark_strbuf_puts(html, "><code>");
    } else {
      bufsize_t first_tag = 0;
      while (node->as.code.info[first_tag] &&
             !cmark_isspace(node->as.code.info[first_tag])) {
        first_tag += 1;
      }

      cmark_strbuf_puts(html, "<pre");
      S_render_sourcepos(node, html, options);
      cmark_strbuf_puts(html, "><code class=\"");
      if (strncmp((char *)node->as.code.info, "language-", 9) != 0) {
        cmark_strbuf_puts(html, "language-");
      }
      escape_html(html, node->as.code.info, first_tag);
      cmark_strbuf_puts(html, "\">");
    }

    escape_html(html, node->data, node->len);
    cmark_strbuf_puts(html, "</code></pre>\n");
    break;

  case CMARK_NODE_HTML_BLOCK:
    cr(html);
    if (!(options & CMARK_OPT_UNSAFE)) {
      cmark_strbuf_puts(html, "<!-- raw HTML omitted -->");
    } else {
      cmark_strbuf_put(html, node->data, node->len);
    }
    cr(html);
    break;

  case CMARK_NODE_CUSTOM_BLOCK: {
    unsigned char *block = entering ? node->as.custom.on_enter :
                                      node->as.custom.on_exit;
    cr(html);
    if (block) {
      cmark_strbuf_puts(html, (char *)block);
    }
    cr(html);
    break;
  }

  case CMARK_NODE_THEMATIC_BREAK:
    cr(html);
    cmark_strbuf_puts(html, "<hr");
    S_render_sourcepos(node, html, options);
    cmark_strbuf_puts(html, " />\n");
    break;

  case CMARK_NODE_PARAGRAPH:
    parent = cmark_node_parent(node);
    grandparent = cmark_node_parent(parent);
    if (grandparent != NULL && grandparent->type == CMARK_NODE_LIST) {
      tight = grandparent->as.list.tight;
    } else {
      tight = false;
    }
    if (!tight) {
      if (entering) {
        cr(html);
        cmark_strbuf_puts(html, "<p");
        S_render_sourcepos(node, html, options);
        cmark_strbuf_putc(html, '>');
      } else {
        cmark_strbuf_puts(html, "</p>\n");
      }
    }
    break;

  case CMARK_NODE_TEXT:
    escape_html(html, node->data, node->len);
    break;

  case CMARK_NODE_LINEBREAK:
    cmark_strbuf_puts(html, "<br />\n");
    break;

  case CMARK_NODE_SOFTBREAK:
    if (options & CMARK_OPT_HARDBREAKS) {
      cmark_strbuf_puts(html, "<br />\n");
    } else if (options & CMARK_OPT_NOBREAKS) {
      cmark_strbuf_putc(html, ' ');
    } else {
      cmark_strbuf_putc(html, '\n');
    }
    break;

  case CMARK_NODE_CODE:
    cmark_strbuf_puts(html, "<code>");
    escape_html(html, node->data, node->len);
    cmark_strbuf_puts(html, "</code>");
    break;

  case CMARK_NODE_HTML_INLINE:
    if (!(options & CMARK_OPT_UNSAFE)) {
      cmark_strbuf_puts(html, "<!-- raw HTML omitted -->");
    } else {
      cmark_strbuf_put(html, node->data, node->len);
    }
    break;

  case CMARK_NODE_CUSTOM_INLINE: {
    unsigned char *block = entering ? node->as.custom.on_enter :
                                      node->as.custom.on_exit;
    if (block) {
      cmark_strbuf_puts(html, (char *)block);
    }
    break;
  }

  case CMARK_NODE_STRONG:
    if (entering) {
      cmark_strbuf_puts(html, "<strong>");
    } else {
      cmark_strbuf_puts(html, "</strong>");
    }
    break;

  case CMARK_NODE_STRIKETHROUGH:
    if (entering) {
      cmark_strbuf_puts(html, "<s>");
    } else {
      cmark_strbuf_puts(html, "</s>");
    }
    break;

  case CMARK_NODE_MARK:
    if (entering) {
      cmark_strbuf_puts(html, "<mark>");
    } else {
      cmark_strbuf_puts(html, "</mark>");
    }
    break;

  case CMARK_NODE_WIKILINK:
    if (entering) {
      cmark_strbuf_puts(html, "<a class=\"wikilink\" href=\"");
      if (node->as.link.url) {
        escape_html(html, node->as.link.url,
                    (bufsize_t)strlen((char *)node->as.link.url));
      }
      cmark_strbuf_puts(html, "\">");
    } else {
      cmark_strbuf_puts(html, "</a>");
    }
    break;

  case CMARK_NODE_EMPH:
    if (entering) {
      cmark_strbuf_puts(html, "<em>");
    } else {
      cmark_strbuf_puts(html, "</em>");
    }
    break;

  case CMARK_NODE_LINK:
    if (entering) {
      cmark_strbuf_puts(html, "<a href=\"");
      if (node->as.link.url && ((options & CMARK_OPT_UNSAFE) ||
                                !(_scan_dangerous_url(node->as.link.url)))) {
        houdini_escape_href(html, node->as.link.url,
                            (bufsize_t)strlen((char *)node->as.link.url));
      }
      if (node->as.link.title) {
        cmark_strbuf_puts(html, "\" title=\"");
        escape_html(html, node->as.link.title,
                    (bufsize_t)strlen((char *)node->as.link.title));
      }
      cmark_strbuf_puts(html, "\">");
    } else {
      cmark_strbuf_puts(html, "</a>");
    }
    break;

  case CMARK_NODE_IMAGE:
    if (entering) {
      cmark_strbuf_puts(html, "<img src=\"");
      if (node->as.link.url && ((options & CMARK_OPT_UNSAFE) ||
                                !(_scan_dangerous_url(node->as.link.url)))) {
        houdini_escape_href(html, node->as.link.url,
                            (bufsize_t)strlen((char *)node->as.link.url));
      }
      cmark_strbuf_puts(html, "\" alt=\"");
      state->plain = node;
    } else {
      if (node->as.link.title) {
        cmark_strbuf_puts(html, "\" title=\"");
        escape_html(html, node->as.link.title,
                    (bufsize_t)strlen((char *)node->as.link.title));
      }

      cmark_strbuf_puts(html, "\" />");
    }
    break;

  case CMARK_NODE_FRONTMATTER:
    break;

  case CMARK_NODE_FOOTNOTE_REFERENCE: {
    int number = node->as.footnote_ref.number;
    snprintf(buffer, BUFFER_SIZE,
             "<sup class=\"footnote-ref\" id=\"fnref-%d\">"
             "<a href=\"#fn-%d\">%d</a></sup>",
             number, number, number);
    cmark_strbuf_puts(html, buffer);
    break;
  }

  case CMARK_NODE_FOOTNOTE_DEFINITION: {
    int number = node->as.footnote_def.number;
    if (number == 0)
      return 0;
    if (entering) {
      snprintf(buffer, BUFFER_SIZE, "<li id=\"fn-%d\" value=\"%d\">\n", number, number);
      cmark_strbuf_puts(state->footnotes_html, buffer);
      state->in_footnote = true;
    } else {
      cmark_strbuf *fn = state->footnotes_html;
      snprintf(buffer, BUFFER_SIZE,
               " <a href=\"#fnref-%d\" class=\"footnote-backref\">"
               "\xe2\x86\xa9</a>",
               number);
      if (fn->size >= 5 &&
          memcmp(fn->ptr + fn->size - 5, "</p>\n", 4) == 0) {
        fn->size -= 5;
        fn->ptr[fn->size] = 0;
        cmark_strbuf_puts(fn, buffer);
        cmark_strbuf_puts(fn, "</p>\n");
      } else {
        cmark_strbuf_puts(fn, buffer);
        cmark_strbuf_putc(fn, '\n');
      }
      cmark_strbuf_puts(fn, "</li>\n");
      state->in_footnote = false;
    }
    break;
  }

  case CMARK_NODE_INLINE_FOOTNOTE: {
    int number = node->as.footnote_ref.number;
    if (entering) {
      snprintf(buffer, BUFFER_SIZE,
               "<sup class=\"footnote-ref\" id=\"fnref-%d\">"
               "<a href=\"#fn-%d\">%d</a></sup>",
               number, number, number);
      cmark_strbuf_puts(state->html, buffer);
      snprintf(buffer, BUFFER_SIZE, "<li id=\"fn-%d\" value=\"%d\">\n<p>", number, number);
      cmark_strbuf_puts(state->footnotes_html, buffer);
      state->in_footnote = true;
    } else {
      snprintf(buffer, BUFFER_SIZE,
               " <a href=\"#fnref-%d\" class=\"footnote-backref\">"
               "\xe2\x86\xa9</a></p>\n</li>\n",
               number);
      cmark_strbuf_puts(state->footnotes_html, buffer);
      state->in_footnote = false;
    }
    break;
  }

  case CMARK_NODE_TABLE:
    if (entering) {
      cr(html);
      cmark_strbuf_puts(html, "<table");
      S_render_sourcepos(node, html, options);
      cmark_strbuf_puts(html, ">\n");
    } else {
      if (node->last_child &&
          node->last_child->as.table_row.type == CMARK_TABLE_ROW_TYPE_DATA) {
        cmark_strbuf_puts(html, "</tbody>\n");
      }
      cmark_strbuf_puts(html, "</table>\n");
    }
    break;

  case CMARK_NODE_TABLE_ROW:
    switch (node->as.table_row.type) {
    case CMARK_TABLE_ROW_TYPE_HEADER:
      if (entering) {
        cmark_strbuf_puts(html, "<thead>\n<tr");
        S_render_sourcepos(node, html, options);
        cmark_strbuf_puts(html, ">\n");
      } else {
        cmark_strbuf_puts(html, "</tr>\n</thead>\n");
      }
      break;
    case CMARK_TABLE_ROW_TYPE_DELIMITER:
      if (entering) {
        return 0;
      }
      break;
    case CMARK_TABLE_ROW_TYPE_DATA:
      if (entering) {
        if (node->prev &&
            node->prev->as.table_row.type != CMARK_TABLE_ROW_TYPE_DATA) {
          cmark_strbuf_puts(html, "<tbody>\n");
        }
        cmark_strbuf_puts(html, "<tr");
        S_render_sourcepos(node, html, options);
        cmark_strbuf_puts(html, ">\n");
      } else {
        cmark_strbuf_puts(html, "</tr>\n");
      }
      break;
    }
    break;

  case CMARK_NODE_TABLE_CELL: {
    const char *tag = (node->parent->as.table_row.type ==
                       CMARK_TABLE_ROW_TYPE_HEADER) ? "th" : "td";
    if (entering) {
      cr(html);
      cmark_strbuf_puts(html, "<");
      cmark_strbuf_puts(html, tag);
      if (node->parent && node->parent->parent &&
          node->parent->parent->type == CMARK_NODE_TABLE) {
        int col = node->as.table_cell.idx;
        if (col < node->parent->parent->as.table.columns_cnt) {
          switch (node->parent->parent->as.table.alignments[col]) {
          case CMARK_TABLE_ALIGN_LEFT:
            cmark_strbuf_puts(html, " style=\"text-align: left\"");
            break;
          case CMARK_TABLE_ALIGN_CENTER:
            cmark_strbuf_puts(html, " style=\"text-align: center\"");
            break;
          case CMARK_TABLE_ALIGN_RIGHT:
            cmark_strbuf_puts(html, " style=\"text-align: right\"");
            break;
          case CMARK_TABLE_ALIGN_NONE:
            break;
          }
        }
      }
      S_render_sourcepos(node, html, options);
      cmark_strbuf_putc(html, '>');
    } else {
      cmark_strbuf_puts(html, "</");
      cmark_strbuf_puts(html, tag);
      cmark_strbuf_puts(html, ">\n");
    }
    break;
  }

  case CMARK_NODE_FORMULA_INLINE: {
    bool display = cmark_node_get_formula_display(node);
    cmark_strbuf_puts(html, "<eq class=\"tex-to-render\">$");
    if (display)
      cmark_strbuf_putc(html, '$');
    escape_inline_formula(html, node->data, node->len);
    if (display)
      cmark_strbuf_putc(html, '$');
    cmark_strbuf_puts(html, "$</eq>");
    break;
  }

  case CMARK_NODE_FORMULA_BLOCK:
    cr(html);
    cmark_strbuf_puts(html, "<section><eqn class=\"tex-to-render\"");
    S_render_sourcepos(node, html, options);
    cmark_strbuf_puts(html, ">$$");
    escape_html(html, node->data, node->len);
    cmark_strbuf_puts(html, "$$</eqn></section>\n");
    break;

  default:
    assert(false);
    break;
  }

  // cmark_strbuf_putc(html, 'x');
  return 1;
}

char *cmark_render_html(cmark_node *root, int options) {
  char *result;
  cmark_strbuf html = CMARK_BUF_INIT(root->mem);
  cmark_strbuf footnotes_buf = CMARK_BUF_INIT(root->mem);
  cmark_event_type ev_type;
  cmark_node *cur;
  struct render_state state = {&html, NULL, &footnotes_buf, false};
  cmark_iter *iter = cmark_iter_new(root);

  while ((ev_type = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
    cur = cmark_iter_get_node(iter);
    if (!S_render_node(cur, ev_type, &state, options)) {
      cmark_iter_reset(iter, cur, CMARK_EVENT_EXIT);
    }
  }

  if (footnotes_buf.size > 0) {
    cmark_strbuf_puts(&html,
                      "<section class=\"footnotes\" data-footnotes>\n"
                      "<hr />\n<ol>\n");
    cmark_strbuf_put(&html, footnotes_buf.ptr, footnotes_buf.size);
    cmark_strbuf_puts(&html, "</ol>\n</section>\n");
  }
  cmark_strbuf_free(&footnotes_buf);

  result = (char *)cmark_strbuf_detach(&html);
  cmark_iter_free(iter);
  return result;
}
