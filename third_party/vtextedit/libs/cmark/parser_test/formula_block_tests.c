#include <cmark.h>

#include "test_utils.h"

int test_formula_block_simple() {
  return test_xml("$$\nE=mc^2\n$$",
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
      "<document xmlns=\"http://commonmark.org/xml/1.0\">\n"
      "  <formula_block xml:space=\"preserve\">E=mc^2\n</formula_block>\n"
      "</document>\n",
      CMARK_OPT_DEFAULT);
}

int test_formula_block_raw() {
  return test_xml("\\begin{equa}\nE=mc^2\n\\end{equa}",
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
      "<document xmlns=\"http://commonmark.org/xml/1.0\">\n"
      "  <formula_block xml:space=\"preserve\">E=mc^2\n</formula_block>\n"
      "</document>\n",
      CMARK_OPT_DEFAULT);
}

int test_formula_block_multiple() {
  return test_xml("$$\na+b\n$$\n\n$$\nc+d\n$$",
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
      "<document xmlns=\"http://commonmark.org/xml/1.0\">\n"
      "  <formula_block xml:space=\"preserve\">a+b\n</formula_block>\n"
      "  <formula_block xml:space=\"preserve\">c+d\n</formula_block>\n"
      "</document>\n",
      CMARK_OPT_DEFAULT);
}

int test_formula_block_with_escape() {
  return test_xml("$$\na\\$\\$b\n$$",
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
      "<document xmlns=\"http://commonmark.org/xml/1.0\">\n"
      "  <formula_block xml:space=\"preserve\">a\\$\\$b\n</formula_block>\n"
      "</document>\n",
      CMARK_OPT_DEFAULT);
}

int test_formula_block_not_closed() {
  return test_xml("$$\nformula",
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
      "<document xmlns=\"http://commonmark.org/xml/1.0\">\n"
      "  <formula_block xml:space=\"preserve\">formula\n</formula_block>\n"
      "</document>\n",
      CMARK_OPT_DEFAULT);
}

int test_formula_block_empty() {
  return test_xml("$$\n$$",
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
      "<document xmlns=\"http://commonmark.org/xml/1.0\">\n"
      "  <formula_block xml:space=\"preserve\"></formula_block>\n"
      "</document>\n",
      CMARK_OPT_DEFAULT);
}

int test_formula_block_simple_html() {
  return test_html("$$\nE=mc^2\n$$",
      "<section><eqn class=\"tex-to-render\">$$E=mc^2\n$$</eqn></section>\n",
      CMARK_OPT_DEFAULT);
}

int test_formula_block_raw_html() {
  return test_html("\\begin{equa}\nE=mc^2\n\\end{equa}",
      "<section><eqn class=\"tex-to-render\">$$E=mc^2\n$$</eqn></section>\n",
      CMARK_OPT_DEFAULT);
}

int test_formula_block_sourcepos_html() {
  return test_html("$$\nE=mc^2\n$$",
      "<section><eqn class=\"tex-to-render\" data-sourcepos=\"1:1-3:2\">"
      "$$E=mc^2\n$$</eqn></section>\n",
      CMARK_OPT_SOURCEPOS);
}

int test_formula_block_html_metacharacters() {
  return test_html("$$\na < b & c > d\n$$",
      "<section><eqn class=\"tex-to-render\">"
      "$$a &lt; b &amp; c &gt; d\n$$</eqn></section>\n",
      CMARK_OPT_DEFAULT);
}

int test_formula_block_escaped_dollars_html() {
  return test_html("$$\na\\$\\$b\n$$",
      "<section><eqn class=\"tex-to-render\">$$a\\$\\$b\n$$</eqn></section>\n",
      CMARK_OPT_DEFAULT);
}

int test_formula_block_empty_html() {
  return test_html("$$\n$$",
      "<section><eqn class=\"tex-to-render\">$$$$</eqn></section>\n",
      CMARK_OPT_DEFAULT);
}

int main() {
  CASE(test_formula_block_simple);
  CASE(test_formula_block_raw);
  CASE(test_formula_block_multiple);
  CASE(test_formula_block_with_escape);
  CASE(test_formula_block_not_closed);
  CASE(test_formula_block_empty);
  CASE(test_formula_block_simple_html);
  CASE(test_formula_block_raw_html);
  CASE(test_formula_block_sourcepos_html);
  CASE(test_formula_block_html_metacharacters);
  CASE(test_formula_block_escaped_dollars_html);
  CASE(test_formula_block_empty_html);
  return 0;
}
