#include <stdio.h>
#include <string.h>

#include <cmark.h>

#include "test_utils.h"

int test_formula_inline_simple() {
  return test_xml("$E=mc^2$",
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
      "<document xmlns=\"http://commonmark.org/xml/1.0\">\n"
      "  <paragraph>\n"
      "    <formula_inline xml:space=\"preserve\">E=mc^2</formula_inline>\n"
      "  </paragraph>\n"
      "</document>\n",
      CMARK_OPT_DEFAULT);
}

int test_formula_inline_multiple() {
  return test_xml("$a+b$ and $c+d$",
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
      "<document xmlns=\"http://commonmark.org/xml/1.0\">\n"
      "  <paragraph>\n"
      "    <formula_inline xml:space=\"preserve\">a+b</formula_inline>\n"
      "    <text xml:space=\"preserve\"> and </text>\n"
      "    <formula_inline xml:space=\"preserve\">c+d</formula_inline>\n"
      "  </paragraph>\n"
      "</document>\n",
      CMARK_OPT_DEFAULT);
}

int test_formula_inline_with_escape() {
  return test_xml("$a\\$b$",
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
      "<document xmlns=\"http://commonmark.org/xml/1.0\">\n"
      "  <paragraph>\n"
      "    <formula_inline xml:space=\"preserve\">a$b</formula_inline>\n"
      "  </paragraph>\n"
      "</document>\n",
      CMARK_OPT_DEFAULT);
}

int test_formula_inline_not_closed() {
  return test_xml("$formula",
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
      "<document xmlns=\"http://commonmark.org/xml/1.0\">\n"
      "  <paragraph>\n"
      "    <text xml:space=\"preserve\">$formula</text>\n"
      "  </paragraph>\n"
      "</document>\n",
      CMARK_OPT_DEFAULT);
}

int test_formula_inline_empty() {
  return test_xml("abc $$",
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
      "<document xmlns=\"http://commonmark.org/xml/1.0\">\n"
      "  <paragraph>\n"
      "    <text xml:space=\"preserve\">abc $$</text>\n"
      "  </paragraph>\n"
      "</document>\n",
      CMARK_OPT_DEFAULT);
}

int test_formula_inline_simple_html() {
  return test_html("$E=mc^2$",
      "<p><eq class=\"tex-to-render\">$E=mc^2$</eq></p>\n",
      CMARK_OPT_DEFAULT);
}

int test_formula_inline_multiple_html() {
  return test_html("$a+b$ and $c+d$",
      "<p><eq class=\"tex-to-render\">$a+b$</eq> and "
      "<eq class=\"tex-to-render\">$c+d$</eq></p>\n",
      CMARK_OPT_DEFAULT);
}

int test_formula_inline_special_chars_html() {
  return test_html("$a < b$",
      "<p><eq class=\"tex-to-render\">$a &lt; b$</eq></p>\n",
      CMARK_OPT_DEFAULT);
}

int test_formula_inline_escaped_dollar_html() {
  return test_html("$a\\$b$ and $a" "\\\\" "\\$" "b$",
      "<p><eq class=\"tex-to-render\">$a\\$b$</eq> and "
      "<eq class=\"tex-to-render\">$a" "\\\\" "\\$" "b$</eq></p>\n",
      CMARK_OPT_DEFAULT);
}

int test_formula_inline_html_metacharacters() {
  return test_html("$a < b & c > d$ and $\\text{</eq><img>}$ and $&lt;$",
      "<p><eq class=\"tex-to-render\">$a &lt; b &amp; c &gt; d$</eq> and "
      "<eq class=\"tex-to-render\">$\\text{&lt;/eq&gt;&lt;img&gt;}$</eq> and "
      "<eq class=\"tex-to-render\">$&amp;lt;$</eq></p>\n",
      CMARK_OPT_DEFAULT);
}

int test_formula_inline_literal_contexts_html() {
  return test_html("\\$p_1\\$ and `$p_1$`\n\n$$p_1$$\n\n$formula",
      "<p>$p_1$ and <code>$p_1$</code></p>\n"
      "<p><eq class=\"tex-to-render\">$$p_1$$</eq></p>\n"
      "<p>$formula</p>\n",
      CMARK_OPT_DEFAULT);
}

int test_formula_inline_display_xml() {
  return test_xml("$x$ and $$ y $$",
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
      "<document xmlns=\"http://commonmark.org/xml/1.0\">\n"
      "  <paragraph>\n"
      "    <formula_inline xml:space=\"preserve\">x</formula_inline>\n"
      "    <text xml:space=\"preserve\"> and </text>\n"
      "    <formula_inline display=\"true\" xml:space=\"preserve\"> y </formula_inline>\n"
      "  </paragraph>\n"
      "</document>\n",
      CMARK_OPT_DEFAULT);
}

int test_formula_inline_display_contexts_html() {
  return test_html("Before $$a+b$$, $$ c+d $$ and $e$.\n\n"
                   "- item $$x$$$$y$$",
      "<p>Before <eq class=\"tex-to-render\">$$a+b$$</eq>, "
      "<eq class=\"tex-to-render\">$$ c+d $$</eq> and "
      "<eq class=\"tex-to-render\">$e$</eq>.</p>\n"
      "<ul>\n<li>item <eq class=\"tex-to-render\">$$x$$</eq>"
      "<eq class=\"tex-to-render\">$$y$$</eq></li>\n</ul>\n",
      CMARK_OPT_DEFAULT);
}

int test_formula_inline_display_escapes_html() {
  return test_html("\\$\\$x\\$\\$ and `$$x$$` and $$a\\$b$$ and "
                   "$$a" "\\\\" "\\$" "b$$ and $$a" "\\\\" "$$",
      "<p>$$x$$ and <code>$$x$$</code> and "
      "<eq class=\"tex-to-render\">$$a\\$b$$</eq> and "
      "<eq class=\"tex-to-render\">$$a" "\\\\" "\\$" "b$$</eq> and "
      "<eq class=\"tex-to-render\">$$a" "\\\\" "$$</eq></p>\n",
      CMARK_OPT_DEFAULT);
}

int test_formula_inline_display_malformed_html() {
  return test_html("before $$unterminated", "<p>before $$unterminated</p>\n",
                   CMARK_OPT_DEFAULT) &&
         test_html("before $$$$ after", "<p>before $$$$ after</p>\n",
                   CMARK_OPT_DEFAULT) &&
         test_html("before $$a$b", "<p>before $$a$b</p>\n",
                   CMARK_OPT_DEFAULT) &&
         test_html("before $$a$b$$",
                   "<p>before $$a<eq class=\"tex-to-render\">$b$</eq>$</p>\n",
                   CMARK_OPT_DEFAULT) &&
         test_html("before $$x$$2", "<p>before $$x$$2</p>\n",
                   CMARK_OPT_DEFAULT) &&
         test_html("before 2$$x$$", "<p>before 2$$x$$</p>\n",
                   CMARK_OPT_DEFAULT) &&
         test_html("before $$$x$$$", "<p>before $$$x$$$</p>\n",
                   CMARK_OPT_DEFAULT) &&
         test_html("before $$x\n+y$$ after $z$\n",
                   "<p>before $$x\n+y$$ after "
                   "<eq class=\"tex-to-render\">$z$</eq></p>\n",
                   CMARK_OPT_DEFAULT);
}

int test_formula_inline_display_html_metacharacters() {
  return test_html("$$a < b & c > d$$ and $$\\text{</eq><img>}$$ and $$&lt;$$",
      "<p><eq class=\"tex-to-render\">$$a &lt; b &amp; c &gt; d$$</eq> and "
      "<eq class=\"tex-to-render\">$$\\text{&lt;/eq&gt;&lt;img&gt;}$$</eq> and "
      "<eq class=\"tex-to-render\">$$&amp;lt;$$</eq></p>\n",
      CMARK_OPT_DEFAULT);
}

int test_formula_display_api() {
  const char *markdown = "$x$ $$ y $$\n\n$$\nz\n$$";
  const cmark_node_type types[] = {CMARK_NODE_FORMULA_INLINE,
                                   CMARK_NODE_FORMULA_INLINE,
                                   CMARK_NODE_FORMULA_BLOCK};
  const char *literals[] = {"x", " y ", "z\n"};
  const int displays[] = {0, 1, 1};
  cmark_node *doc =
      cmark_parse_document(markdown, strlen(markdown), CMARK_OPT_DEFAULT);
  cmark_iter *iter = cmark_iter_new(doc);
  cmark_event_type event;
  int count = 0;
  int ret = cmark_node_get_formula_display(NULL) == 0 &&
            cmark_node_get_formula_display(doc) == 0;
  while ((event = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
    if (event != CMARK_EVENT_ENTER)
      continue;
    cmark_node *node = cmark_iter_get_node(iter);
    cmark_node_type type = cmark_node_get_type(node);
    if (type != CMARK_NODE_FORMULA_INLINE && type != CMARK_NODE_FORMULA_BLOCK)
      continue;
    if (count >= 3 || type != types[count] ||
        cmark_node_get_formula_display(node) != displays[count] ||
        strcmp(cmark_node_get_literal(node), literals[count]) != 0) {
      ret = 0;
      break;
    }
    ++count;
  }
  ret = ret && count == 3;
  if (!ret)
    fprintf(stderr, "Formula display API did not preserve inline/block modes\n");
  cmark_iter_free(iter);
  cmark_node_free(doc);
  return ret;
}

int test_formula_inline_display_source_bounds() {
  const char *markdown = "\xCE\xB1 $$\xCE\xB2\\$+\xCE\xB3$$ and $x$\n";
  const char *literals[] = {"\xCE\xB2$+\xCE\xB3", "x"};
  const int starts[] = {6, 21};
  const int ends[] = {12, 21};
  cmark_node *doc =
      cmark_parse_document(markdown, strlen(markdown), CMARK_OPT_DEFAULT);
  cmark_iter *iter = cmark_iter_new(doc);
  cmark_event_type event;
  int count = 0;
  int ret = 1;
  while ((event = cmark_iter_next(iter)) != CMARK_EVENT_DONE) {
    if (event != CMARK_EVENT_ENTER)
      continue;
    cmark_node *node = cmark_iter_get_node(iter);
    if (cmark_node_get_type(node) != CMARK_NODE_FORMULA_INLINE)
      continue;
    if (count >= 2 || strcmp(cmark_node_get_literal(node), literals[count]) ||
        cmark_node_get_formula_display(node) != (count == 0) ||
        cmark_node_get_start_line(node) != 1 ||
        cmark_node_get_end_line(node) != 1 ||
        cmark_node_get_start_column(node) != starts[count] ||
        cmark_node_get_end_column(node) != ends[count]) {
      ret = 0;
      break;
    }
    ++count;
  }
  ret = ret && count == 2;
  if (!ret)
    fprintf(stderr, "Formula source bounds must exclude delimiters, not escapes\n");
  cmark_iter_free(iter);
  cmark_node_free(doc);
  return ret;
}

int main() {
  CASE(test_formula_inline_simple);
  CASE(test_formula_inline_multiple);
  CASE(test_formula_inline_with_escape);
  CASE(test_formula_inline_not_closed);
  CASE(test_formula_inline_empty);
  CASE(test_formula_inline_simple_html);
  CASE(test_formula_inline_multiple_html);
  CASE(test_formula_inline_special_chars_html);
  CASE(test_formula_inline_escaped_dollar_html);
  CASE(test_formula_inline_html_metacharacters);
  CASE(test_formula_inline_literal_contexts_html);
  CASE(test_formula_inline_display_xml);
  CASE(test_formula_inline_display_contexts_html);
  CASE(test_formula_inline_display_escapes_html);
  CASE(test_formula_inline_display_malformed_html);
  CASE(test_formula_inline_display_html_metacharacters);
  CASE(test_formula_display_api);
  CASE(test_formula_inline_display_source_bounds);
  return 0;
}
