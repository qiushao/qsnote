#include <stdio.h>
#include <string.h>

#include <cmark.h>
#include "test_utils.h"

int test_table_basic() {
  return test_xml(
    "the text before the table\n\n"
    "| foo | bar | zoo |\n"
    "| :--- | :---: | ---: |\n"
    "| baz | bim | xyz |\n"
    "the text after the table",
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
    "<document sourcepos=\"1:1-6:24\" xmlns=\"http://commonmark.org/xml/1.0\">\n"
    "  <paragraph sourcepos=\"1:1-1:25\">\n"
    "    <text sourcepos=\"1:1-1:25\" xml:space=\"preserve\">the text before the table</text>\n"
    "  </paragraph>\n"
    "  <table sourcepos=\"3:1-5:19\" columns=\"3\">\n"
    "    <table_row sourcepos=\"3:1-3:19\" type=\"header\">\n"
    "      <table_cell sourcepos=\"3:2-3:6\" align=\"left\">\n"
    "        <text sourcepos=\"3:2-3:5\" xml:space=\"preserve\"> foo</text>\n"
    "      </table_cell>\n"
    "      <table_cell sourcepos=\"3:8-3:12\" align=\"center\">\n"
    "        <text sourcepos=\"3:8-3:11\" xml:space=\"preserve\"> bar</text>\n"
    "      </table_cell>\n"
    "      <table_cell sourcepos=\"3:14-3:18\" align=\"right\">\n"
    "        <text sourcepos=\"3:14-3:17\" xml:space=\"preserve\"> zoo</text>\n"
    "      </table_cell>\n"
    "    </table_row>\n"
    "    <table_row sourcepos=\"4:1-4:23\" type=\"delimiter\">\n"
    "      <table_cell sourcepos=\"4:2-4:7\" align=\"left\" />\n"
    "      <table_cell sourcepos=\"4:9-4:15\" align=\"center\" />\n"
    "      <table_cell sourcepos=\"4:17-4:22\" align=\"right\" />\n"
    "    </table_row>\n"
    "    <table_row sourcepos=\"5:1-5:19\" type=\"data\">\n"
    "      <table_cell sourcepos=\"5:2-5:6\" align=\"left\">\n"
    "        <text sourcepos=\"5:2-5:5\" xml:space=\"preserve\"> baz</text>\n"
    "      </table_cell>\n"
    "      <table_cell sourcepos=\"5:8-5:12\" align=\"center\">\n"
    "        <text sourcepos=\"5:8-5:11\" xml:space=\"preserve\"> bim</text>\n"
    "      </table_cell>\n"
    "      <table_cell sourcepos=\"5:14-5:18\" align=\"right\">\n"
    "        <text sourcepos=\"5:14-5:17\" xml:space=\"preserve\"> xyz</text>\n"
    "      </table_cell>\n"
    "    </table_row>\n"
    "  </table>\n"
    "  <paragraph sourcepos=\"6:1-6:24\">\n"
    "    <text sourcepos=\"6:1-6:24\" xml:space=\"preserve\">the text after the table</text>\n"
    "  </paragraph>\n"
    "</document>\n",
    CMARK_OPT_SOURCEPOS);
}

int test_table_alignments() {
  return test_xml(
    "| left | center | right |\n"
    "|:-----|:------:|------:|\n"
    "| a    |    b   |     c |",
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
    "<document xmlns=\"http://commonmark.org/xml/1.0\">\n"
    "  <table columns=\"3\">\n"
    "    <table_row type=\"header\">\n"
    "      <table_cell align=\"left\">\n"
    "        <text xml:space=\"preserve\"> left</text>\n"
    "      </table_cell>\n"
    "      <table_cell align=\"center\">\n"
    "        <text xml:space=\"preserve\"> center</text>\n"
    "      </table_cell>\n"
    "      <table_cell align=\"right\">\n"
    "        <text xml:space=\"preserve\"> right</text>\n"
    "      </table_cell>\n"
    "    </table_row>\n"
    "    <table_row type=\"delimiter\">\n"
    "      <table_cell align=\"left\" />\n"
    "      <table_cell align=\"center\" />\n"
    "      <table_cell align=\"right\" />\n"
    "    </table_row>\n"
    "    <table_row type=\"data\">\n"
    "      <table_cell align=\"left\">\n"
    "        <text xml:space=\"preserve\"> a</text>\n"
    "      </table_cell>\n"
    "      <table_cell align=\"center\">\n"
    "        <text xml:space=\"preserve\">    b</text>\n"
    "      </table_cell>\n"
    "      <table_cell align=\"right\">\n"
    "        <text xml:space=\"preserve\">     c</text>\n"
    "      </table_cell>\n"
    "    </table_row>\n"
    "  </table>\n"
    "</document>\n",
    CMARK_OPT_DEFAULT);
}

int test_table_empty_cells() {
  return test_xml(
    "| a | b |\n"
    "| --- | --- |\n"
    "|  | d |",
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
    "<document xmlns=\"http://commonmark.org/xml/1.0\">\n"
    "  <table columns=\"2\">\n"
    "    <table_row type=\"header\">\n"
    "      <table_cell align=\"none\">\n"
    "        <text xml:space=\"preserve\"> a</text>\n"
    "      </table_cell>\n"
    "      <table_cell align=\"none\">\n"
    "        <text xml:space=\"preserve\"> b</text>\n"
    "      </table_cell>\n"
    "    </table_row>\n"
    "    <table_row type=\"delimiter\">\n"
    "      <table_cell align=\"none\" />\n"
    "      <table_cell align=\"none\" />\n"
    "    </table_row>\n"
    "    <table_row type=\"data\">\n"
    "      <table_cell align=\"none\" />\n"
    "      <table_cell align=\"none\">\n"
    "        <text xml:space=\"preserve\"> d</text>\n"
    "      </table_cell>\n"
    "    </table_row>\n"
    "  </table>\n"
    "</document>\n",
    CMARK_OPT_DEFAULT);
}

int test_table_escaped_pipes() {
  return test_xml(
    "| a \\| b | c |\n"
    "| --- | --- |\n"
    "| d | e \\| f |",
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
    "<document xmlns=\"http://commonmark.org/xml/1.0\">\n"
    "  <table columns=\"2\">\n"
    "    <table_row type=\"header\">\n"
    "      <table_cell align=\"none\">\n"
    "        <text xml:space=\"preserve\"> a | b</text>\n"
    "      </table_cell>\n"
    "      <table_cell align=\"none\">\n"
    "        <text xml:space=\"preserve\"> c</text>\n"
    "      </table_cell>\n"
    "    </table_row>\n"
    "    <table_row type=\"delimiter\">\n"
    "      <table_cell align=\"none\" />\n"
    "      <table_cell align=\"none\" />\n"
    "    </table_row>\n"
    "    <table_row type=\"data\">\n"
    "      <table_cell align=\"none\">\n"
    "        <text xml:space=\"preserve\"> d</text>\n"
    "      </table_cell>\n"
    "      <table_cell align=\"none\">\n"
    "        <text xml:space=\"preserve\"> e | f</text>\n"
    "      </table_cell>\n"
    "    </table_row>\n"
    "  </table>\n"
    "</document>\n",
    CMARK_OPT_DEFAULT);
}

int test_table_invalid_no_header() {
  return test_xml(
    "| a | b |\n"
    "| d | e |",
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
    "<document xmlns=\"http://commonmark.org/xml/1.0\">\n"
    "  <paragraph>\n"
    "    <text xml:space=\"preserve\">| a | b |</text>\n"
    "    <softbreak />\n"
    "    <text xml:space=\"preserve\">| d | e |</text>\n"
    "  </paragraph>\n"
    "</document>\n",
    CMARK_OPT_DEFAULT);
}

int test_table_invalid_delimiter() {
  return test_xml(
    "| a | b |\n"
    "| -- | --- - |\n"
    "| c | d |",
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
    "<document xmlns=\"http://commonmark.org/xml/1.0\">\n"
    "  <paragraph>\n"
    "    <text xml:space=\"preserve\">| a | b |</text>\n"
    "    <softbreak />\n"
    "    <text xml:space=\"preserve\">| -- | --- - |</text>\n"
    "    <softbreak />\n"
    "    <text xml:space=\"preserve\">| c | d |</text>\n"
    "  </paragraph>\n"
    "</document>\n",
    CMARK_OPT_DEFAULT);
}

int test_table_no_leading_pipe() {
  return test_xml(
    "foo | bar |\n"
    "--- | --- |\n"
    "baz | bim |",
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
    "<document xmlns=\"http://commonmark.org/xml/1.0\">\n"
    "  <paragraph>\n"
    "    <text xml:space=\"preserve\">foo | bar |</text>\n"
    "    <softbreak />\n"
    "    <text xml:space=\"preserve\">--- | --- |</text>\n"
    "    <softbreak />\n"
    "    <text xml:space=\"preserve\">baz | bim |</text>\n"
    "  </paragraph>\n"
    "</document>\n",
    CMARK_OPT_DEFAULT);
}

int test_table_mismatched_columns() {
  return test_xml(
    "| a | b | c |\n"
    "| --- | --- |\n"
    "| d | e |",
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
    "<document xmlns=\"http://commonmark.org/xml/1.0\">\n"
    "  <paragraph>\n"
    "    <text xml:space=\"preserve\">| a | b | c |</text>\n"
    "    <softbreak />\n"
    "    <text xml:space=\"preserve\">| --- | --- |</text>\n"
    "    <softbreak />\n"
    "    <text xml:space=\"preserve\">| d | e |</text>\n"
    "  </paragraph>\n"
    "</document>\n",
    CMARK_OPT_DEFAULT);
}

int test_table_with_inline_markdown() {
  return test_xml(
    "| *em* | **strong** |\n"
    "| --- | --- |\n"
    "| `code` | [link](url) |",
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
    "<document sourcepos=\"1:1-3:24\" xmlns=\"http://commonmark.org/xml/1.0\">\n"
    "  <table sourcepos=\"1:1-3:24\" columns=\"2\">\n"
    "    <table_row sourcepos=\"1:1-1:21\" type=\"header\">\n"
    "      <table_cell sourcepos=\"1:2-1:7\" align=\"none\">\n"
    "        <text sourcepos=\"1:2-1:2\" xml:space=\"preserve\"> </text>\n"
    "        <emph sourcepos=\"1:3-1:6\">\n"
    "          <text sourcepos=\"1:4-1:5\" xml:space=\"preserve\">em</text>\n"
    "        </emph>\n"
    "      </table_cell>\n"
    "      <table_cell sourcepos=\"1:9-1:20\" align=\"none\">\n"
    "        <text sourcepos=\"1:9-1:9\" xml:space=\"preserve\"> </text>\n"
    "        <strong sourcepos=\"1:10-1:19\">\n"
    "          <text sourcepos=\"1:12-1:17\" xml:space=\"preserve\">strong</text>\n"
    "        </strong>\n"
    "      </table_cell>\n"
    "    </table_row>\n"
    "    <table_row sourcepos=\"2:1-2:13\" type=\"delimiter\">\n"
    "      <table_cell sourcepos=\"2:2-2:6\" align=\"none\" />\n"
    "      <table_cell sourcepos=\"2:8-2:12\" align=\"none\" />\n"
    "    </table_row>\n"
    "    <table_row sourcepos=\"3:1-3:24\" type=\"data\">\n"
    "      <table_cell sourcepos=\"3:2-3:9\" align=\"none\">\n"
    "        <text sourcepos=\"3:2-3:2\" xml:space=\"preserve\"> </text>\n"
    "        <code sourcepos=\"3:3-3:8\" xml:space=\"preserve\">code</code>\n"
    "      </table_cell>\n"
    "      <table_cell sourcepos=\"3:11-3:23\" align=\"none\">\n"
    "        <text sourcepos=\"3:11-3:11\" xml:space=\"preserve\"> </text>\n"
    "        <link sourcepos=\"3:12-3:22\" destination=\"url\" urlpos=\"3:19-3:21\">\n"
    "          <text sourcepos=\"3:13-3:16\" xml:space=\"preserve\">link</text>\n"
    "        </link>\n"
    "      </table_cell>\n"
    "    </table_row>\n"
    "  </table>\n"
    "</document>\n",
    CMARK_OPT_SOURCEPOS);
}

int test_table_basic_html() {
  return test_html(
    "the text before the table\n\n"
    "| foo | bar | zoo |\n"
    "| :--- | :---: | ---: |\n"
    "| baz | bim | xyz |\n"
    "the text after the table",
    "<p>the text before the table</p>\n"
    "<table>\n"
    "<thead>\n"
    "<tr>\n"
    "<th style=\"text-align: left\"> foo</th>\n"
    "<th style=\"text-align: center\"> bar</th>\n"
    "<th style=\"text-align: right\"> zoo</th>\n"
    "</tr>\n"
    "</thead>\n"
    "<tbody>\n"
    "<tr>\n"
    "<td style=\"text-align: left\"> baz</td>\n"
    "<td style=\"text-align: center\"> bim</td>\n"
    "<td style=\"text-align: right\"> xyz</td>\n"
    "</tr>\n"
    "</tbody>\n"
    "</table>\n"
    "<p>the text after the table</p>\n",
    CMARK_OPT_DEFAULT);
}

int test_table_alignments_html() {
  return test_html(
    "| left | center | right |\n"
    "|:-----|:------:|------:|\n"
    "| a    |    b   |     c |",
    "<table>\n"
    "<thead>\n"
    "<tr>\n"
    "<th style=\"text-align: left\"> left</th>\n"
    "<th style=\"text-align: center\"> center</th>\n"
    "<th style=\"text-align: right\"> right</th>\n"
    "</tr>\n"
    "</thead>\n"
    "<tbody>\n"
    "<tr>\n"
    "<td style=\"text-align: left\"> a</td>\n"
    "<td style=\"text-align: center\">    b</td>\n"
    "<td style=\"text-align: right\">     c</td>\n"
    "</tr>\n"
    "</tbody>\n"
    "</table>\n",
    CMARK_OPT_DEFAULT);
}

int test_table_no_alignment_html() {
  return test_html(
    "| a | b |\n"
    "| --- | --- |\n"
    "|  | d |",
    "<table>\n"
    "<thead>\n"
    "<tr>\n"
    "<th> a</th>\n"
    "<th> b</th>\n"
    "</tr>\n"
    "</thead>\n"
    "<tbody>\n"
    "<tr>\n"
    "<td></td>\n"
    "<td> d</td>\n"
    "</tr>\n"
    "</tbody>\n"
    "</table>\n",
    CMARK_OPT_DEFAULT);
}

int test_table_sourcepos_html() {
  return test_html(
    "the text before the table\n\n"
    "| foo | bar | zoo |\n"
    "| :--- | :---: | ---: |\n"
    "| baz | bim | xyz |\n"
    "the text after the table",
    "<p data-sourcepos=\"1:1-1:25\">the text before the table</p>\n"
    "<table data-sourcepos=\"3:1-5:19\">\n"
    "<thead>\n"
    "<tr data-sourcepos=\"3:1-3:19\">\n"
    "<th style=\"text-align: left\" data-sourcepos=\"3:2-3:6\"> foo</th>\n"
    "<th style=\"text-align: center\" data-sourcepos=\"3:8-3:12\"> bar</th>\n"
    "<th style=\"text-align: right\" data-sourcepos=\"3:14-3:18\"> zoo</th>\n"
    "</tr>\n"
    "</thead>\n"
    "<tbody>\n"
    "<tr data-sourcepos=\"5:1-5:19\">\n"
    "<td style=\"text-align: left\" data-sourcepos=\"5:2-5:6\"> baz</td>\n"
    "<td style=\"text-align: center\" data-sourcepos=\"5:8-5:12\"> bim</td>\n"
    "<td style=\"text-align: right\" data-sourcepos=\"5:14-5:18\"> xyz</td>\n"
    "</tr>\n"
    "</tbody>\n"
    "</table>\n"
    "<p data-sourcepos=\"6:1-6:24\">the text after the table</p>\n",
    CMARK_OPT_SOURCEPOS);
}

int test_table_inline_html() {
  return test_html(
    "| *em* | **strong** |\n"
    "| --- | --- |\n"
    "| \x60" "code\x60 | [link](url) |",
    "<table>\n"
    "<thead>\n"
    "<tr>\n"
    "<th> <em>em</em></th>\n"
    "<th> <strong>strong</strong></th>\n"
    "</tr>\n"
    "</thead>\n"
    "<tbody>\n"
    "<tr>\n"
    "<td> <code>code</code></td>\n"
    "<td> <a href=\"url\">link</a></td>\n"
    "</tr>\n"
    "</tbody>\n"
    "</table>\n",
    CMARK_OPT_DEFAULT);
}

int test_table_empty_cells_html() {
  return test_html(
    "| a | b |\n"
    "| --- | --- |\n"
    "|  | d |",
    "<table>\n"
    "<thead>\n"
    "<tr>\n"
    "<th> a</th>\n"
    "<th> b</th>\n"
    "</tr>\n"
    "</thead>\n"
    "<tbody>\n"
    "<tr>\n"
    "<td></td>\n"
    "<td> d</td>\n"
    "</tr>\n"
    "</tbody>\n"
    "</table>\n",
    CMARK_OPT_DEFAULT);
}

int test_table_escaped_pipes_html() {
  return test_html(
    "| a \\| b | c |\n"
    "| --- | --- |\n"
    "| d | e \\| f |",
    "<table>\n"
    "<thead>\n"
    "<tr>\n"
    "<th> a | b</th>\n"
    "<th> c</th>\n"
    "</tr>\n"
    "</thead>\n"
    "<tbody>\n"
    "<tr>\n"
    "<td> d</td>\n"
    "<td> e | f</td>\n"
    "</tr>\n"
    "</tbody>\n"
    "</table>\n",
    CMARK_OPT_DEFAULT);
}

int test_table_in_list_item() {
  return test_xml(
    "- text\n\n"
    "    | header |\n"
    "    |:---|\n"
    "    | data |",
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
    "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
    "<document xmlns=\"http://commonmark.org/xml/1.0\">\n"
    "  <list type=\"bullet\" tight=\"false\">\n"
    "    <item>\n"
    "      <paragraph>\n"
    "        <text xml:space=\"preserve\">text</text>\n"
    "      </paragraph>\n"
    "      <table columns=\"1\">\n"
    "        <table_row type=\"header\">\n"
    "          <table_cell align=\"left\">\n"
    "            <text xml:space=\"preserve\"> header</text>\n"
    "          </table_cell>\n"
    "        </table_row>\n"
    "        <table_row type=\"delimiter\">\n"
    "          <table_cell align=\"left\" />\n"
    "        </table_row>\n"
    "        <table_row type=\"data\">\n"
    "          <table_cell align=\"left\">\n"
    "            <text xml:space=\"preserve\"> data</text>\n"
    "          </table_cell>\n"
    "        </table_row>\n"
    "      </table>\n"
    "    </item>\n"
    "  </list>\n"
    "</document>\n",
    CMARK_OPT_DEFAULT);
}

int test_table_in_list_item_html() {
  return test_html(
    "- text\n\n"
    "    | header |\n"
    "    |:---|\n"
    "    | data |",
    "<ul>\n"
    "<li>\n"
    "<p>text</p>\n"
    "<table>\n"
    "<thead>\n"
    "<tr>\n"
    "<th style=\"text-align: left\"> header</th>\n"
    "</tr>\n"
    "</thead>\n"
    "<tbody>\n"
    "<tr>\n"
    "<td style=\"text-align: left\"> data</td>\n"
    "</tr>\n"
    "</tbody>\n"
    "</table>\n"
    "</li>\n"
    "</ul>\n",
    CMARK_OPT_DEFAULT);
}

int test_table_display_math_html() {
  return test_html(
    "| $$h$$ | $i$ |\n"
    "| --- | --- |\n"
    "| $$a$$ and $$ b $$ | `$$code$$` and \\$\\$text\\$\\$ |",
    "<table>\n<thead>\n<tr>\n"
    "<th> <eq class=\"tex-to-render\">$$h$$</eq></th>\n"
    "<th> <eq class=\"tex-to-render\">$i$</eq></th>\n"
    "</tr>\n</thead>\n<tbody>\n<tr>\n"
    "<td> <eq class=\"tex-to-render\">$$a$$</eq> and "
    "<eq class=\"tex-to-render\">$$ b $$</eq></td>\n"
    "<td> <code>$$code$$</code> and $$text$$</td>\n"
    "</tr>\n</tbody>\n</table>\n",
    CMARK_OPT_DEFAULT);
}

int test_table_display_math_boundaries_html() {
  return test_html(
    "| $$left | right$$ |\n"
    "| --- | --- |\n"
    "| $$a\\|b$$ | $$c < d & e$$ |\n"
    "| $$ | $$ |",
    "<table>\n<thead>\n<tr>\n"
    "<th> $$left</th>\n<th> right$$</th>\n"
    "</tr>\n</thead>\n<tbody>\n<tr>\n"
    "<td> <eq class=\"tex-to-render\">$$a\\|b$$</eq></td>\n"
    "<td> <eq class=\"tex-to-render\">$$c &lt; d &amp; e$$</eq></td>\n"
    "</tr>\n<tr>\n<td> $$</td>\n<td> $$</td>\n"
    "</tr>\n</tbody>\n</table>\n",
    CMARK_OPT_DEFAULT);
}

int test_table_display_math_cell_local_source_bounds() {
  const char *markdown =
      "| \xCE\xB1 $$\xCE\xB2\\$c$$ and $d$ | $$e$$ $$f$$ |\n"
      "| --- | --- |\n"
      "| tail | end |";
  const char *cells[] = {" \xCE\xB1 $$\xCE\xB2\\$c$$ and $d$ ",
                         " $$e$$ $$f$$ "};
  cmark_node *doc =
      cmark_parse_document(markdown, strlen(markdown), CMARK_OPT_DEFAULT);
  cmark_node *table = cmark_node_first_child(doc);
  int ret = cmark_node_get_type(table) == CMARK_NODE_TABLE;
  cmark_node *row = cmark_node_first_child(table);
  cmark_node *cell = cmark_node_first_child(row);
  for (int i = 0; ret && i < 2; ++i, cell = cmark_node_next(cell)) {
    cmark_node *local =
        cmark_parse_document(cells[i], strlen(cells[i]), CMARK_OPT_DEFAULT);
    cmark_node *a = cmark_node_first_child(cell);
    cmark_node *b = cmark_node_first_child(cmark_node_first_child(local));
    int offset = cmark_node_get_start_column(cell) - 1;
    int formulas = 0;
    for (; ret; a = cmark_node_next(a), b = cmark_node_next(b)) {
      while (a && cmark_node_get_type(a) != CMARK_NODE_FORMULA_INLINE)
        a = cmark_node_next(a);
      while (b && cmark_node_get_type(b) != CMARK_NODE_FORMULA_INLINE)
        b = cmark_node_next(b);
      if (a == NULL || b == NULL)
        break;
      ++formulas;
      ret = ret &&
            cmark_node_get_formula_display(a) == (i == 1 || formulas == 1) &&
            cmark_node_get_formula_display(a) == cmark_node_get_formula_display(b) &&
            strcmp(cmark_node_get_literal(a), cmark_node_get_literal(b)) == 0 &&
            cmark_node_get_start_line(a) == 1 &&
            cmark_node_get_end_line(a) == 1 &&
            cmark_node_get_start_column(a) == cmark_node_get_start_column(b) + offset &&
            cmark_node_get_end_column(a) == cmark_node_get_end_column(b) + offset;
    }
    ret = ret && !a && !b && formulas == 2;
    cmark_node_free(local);
  }
  if (!ret)
    fprintf(stderr, "Table and cell-local formula parsing must agree on byte bounds\n");
  cmark_node_free(doc);
  return ret;
}

// A table interrupts the last line of a paragraph, not the whole paragraph.
int test_table_interrupts_list_paragraph_html() {
  return test_html(
    "- before\n"
    "- item\n"
    "  continuation\n"
    "  | A | B |\n"
    "  | --- | --- |\n"
    "  | 1 | 2 |\n"
    "- after\n",
    "<ul>\n<li>before</li>\n<li>item\ncontinuation\n"
    "<table>\n<thead>\n<tr>\n<th> A</th>\n<th> B</th>\n</tr>\n</thead>\n"
    "<tbody>\n<tr>\n<td> 1</td>\n<td> 2</td>\n</tr>\n</tbody>\n</table>\n"
    "</li>\n<li>after</li>\n</ul>\n",
    CMARK_OPT_DEFAULT);
}

int test_table_interrupt_source_positions() {
  // Vary the quote prefix width: columns must come from each source line,
  // not the first paragraph line or the table header's indentation.
  const char *markdown =
      "> 1. intro\r\n"
      ">    \xCE\xB1 continuation\r\n"
      ">      | A | B |\r\n"
      ">    | --- | --- |\r\n"
      ">     | 1 | 2 |\r\n"
      "> 2. after";
  cmark_parser *parser = cmark_parser_new(CMARK_OPT_DEFAULT);
  // Exercise CRLF and UTF-8 split across feed boundaries.
  const size_t length = strlen(markdown);
  for (size_t i = 0; i < length; ++i)
    cmark_parser_feed(parser, markdown + i, 1);
  cmark_node *doc = cmark_parser_finish(parser);
  cmark_parser_free(parser);
  cmark_node *quote = cmark_node_first_child(doc);
  cmark_node *list = cmark_node_first_child(quote);
  cmark_node *item = cmark_node_first_child(list);
  cmark_node *paragraph = cmark_node_first_child(item);
  cmark_node *table = cmark_node_next(paragraph);
  int ret = cmark_node_get_type(quote) == CMARK_NODE_BLOCK_QUOTE &&
            cmark_node_get_type(list) == CMARK_NODE_LIST &&
            cmark_node_get_list_type(list) == CMARK_ORDERED_LIST &&
            cmark_node_get_list_tight(list) &&
            cmark_node_get_type(cmark_node_next(item)) == CMARK_NODE_ITEM &&
            cmark_node_get_type(paragraph) == CMARK_NODE_PARAGRAPH &&
            cmark_node_get_start_line(paragraph) == 1 &&
            cmark_node_get_start_column(paragraph) == 6 &&
            cmark_node_get_end_line(paragraph) == 2 &&
            cmark_node_get_end_column(paragraph) == 20 &&
            cmark_node_get_type(table) == CMARK_NODE_TABLE &&
            cmark_node_get_start_line(table) == 3 &&
            cmark_node_get_start_column(table) == 8 &&
            cmark_node_get_end_line(table) == 5 &&
            cmark_node_get_end_column(table) == 15;
  const int columns[] = {8, 6, 7};
  cmark_node *row = cmark_node_first_child(table);
  for (int i = 0; ret && i < 3; ++i, row = cmark_node_next(row)) {
    cmark_node *cell = cmark_node_first_child(row);
    ret = cmark_node_get_type(row) == CMARK_NODE_TABLE_ROW &&
          cmark_node_get_start_line(row) == i + 3 &&
          cmark_node_get_end_line(row) == i + 3 &&
          cmark_node_get_start_column(row) == columns[i] &&
          cmark_node_get_start_line(cell) == i + 3 &&
          cmark_node_get_start_column(cell) == columns[i] + 1;
  }
  ret = ret && row == NULL && cmark_node_next(table) == NULL;
  if (!ret)
    fprintf(stderr, "Interrupted table must preserve nesting and exact source bounds\n");
  cmark_node_free(doc);
  return ret;
}

int test_table_interrupt_keeps_preceding_lines() {
  // The first line looks like a one-column header, but only the final line
  // may be paired with the two-column delimiter.
  return test_html(
    "| preceding |\ntext\n| A | B |\n| --- | --- |\n",
    "<p>| preceding |\ntext</p>\n"
    "<table>\n<thead>\n<tr>\n<th> A</th>\n<th> B</th>\n</tr>\n</thead>\n</table>\n",
    CMARK_OPT_DEFAULT);
}

int test_table_interrupt_rejections() {
  const char *inputs[] = {
      // A valid first line must not mask an invalid final header.
      "| first |\nnot a header\n| --- |\n",
      "text\n| A | B |\n| --- |\n",
      "text\n| A |\n| invalid |\n",
      "text\n| A |\n\n| --- |\n",
      "```\ntext\n| A |\n| --- |\n```\n",
      "- item\n\n      | A |\n      | --- |\n",
  };
  int ret = 1;
  for (size_t i = 0; i < sizeof(inputs) / sizeof(inputs[0]); ++i) {
    cmark_node *doc = cmark_parse_document(inputs[i], strlen(inputs[i]), CMARK_OPT_DEFAULT);
    cmark_iter *iter = cmark_iter_new(doc);
    while (cmark_iter_next(iter) != CMARK_EVENT_DONE) {
      if (cmark_node_get_type(cmark_iter_get_node(iter)) == CMARK_NODE_TABLE) {
        fprintf(stderr, "Unexpected table for rejection case %zu\n", i);
        ret = 0;
      }
    }
    cmark_iter_free(iter);
    cmark_node_free(doc);
  }
  ret = test_html("text\n| A |\n| --- |\n",
                  "<p>text\n| A |\n| --- |</p>\n", CMARK_OPT_NO_EXTENSIONS) && ret;
  return ret;
}

int test_table_interrupt_preserves_reference_definitions() {
  int ret = test_html(
    "[ref]: /target\ntext\n| A |\n| --- |\n| [link][ref] |\n",
    "<p>text</p>\n<table>\n<thead>\n<tr>\n<th> A</th>\n</tr>\n</thead>\n"
    "<tbody>\n<tr>\n<td> <a href=\"/target\">link</a></td>\n</tr>\n</tbody>\n</table>\n",
    CMARK_OPT_DEFAULT);
  // An all-definition prefix is removed entirely, without orphaning the table.
  return test_html(
    "[ref]: /target\n| [link][ref] |\n| --- |\n",
    "<table>\n<thead>\n<tr>\n<th> <a href=\"/target\">link</a></th>\n"
    "</tr>\n</thead>\n</table>\n", CMARK_OPT_DEFAULT) && ret;
}

int main() {
  CASE(test_table_interrupts_list_paragraph_html);
  CASE(test_table_interrupt_source_positions);
  CASE(test_table_interrupt_keeps_preceding_lines);
  CASE(test_table_interrupt_rejections);
  CASE(test_table_interrupt_preserves_reference_definitions);
  CASE(test_table_basic);
  CASE(test_table_alignments);
  CASE(test_table_empty_cells);
  CASE(test_table_escaped_pipes);
  CASE(test_table_invalid_no_header);
  CASE(test_table_invalid_delimiter);
  CASE(test_table_no_leading_pipe);
  CASE(test_table_mismatched_columns);
  CASE(test_table_with_inline_markdown);
  CASE(test_table_basic_html);
  CASE(test_table_alignments_html);
  CASE(test_table_no_alignment_html);
  CASE(test_table_sourcepos_html);
  CASE(test_table_inline_html);
  CASE(test_table_empty_cells_html);
  CASE(test_table_escaped_pipes_html);
  CASE(test_table_in_list_item);
  CASE(test_table_in_list_item_html);
  CASE(test_table_display_math_html);
  CASE(test_table_display_math_boundaries_html);
  CASE(test_table_display_math_cell_local_source_bounds);
  return 0;
}
