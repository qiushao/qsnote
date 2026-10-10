#include <cmark.h>

#include "test_utils.h"

int test_frontmatter_simple() {
  return test_xml("---\ntitle: hello\n---\n\nparagraph",
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
      "<document xmlns=\"http://commonmark.org/xml/1.0\">\n"
      "  <frontmatter xml:space=\"preserve\">title: hello\n</frontmatter>\n"
      "  <paragraph>\n"
      "    <text xml:space=\"preserve\">paragraph</text>\n"
      "  </paragraph>\n"
      "</document>\n",
      CMARK_OPT_DEFAULT);
}

int test_frontmatter_dots_closer() {
  return test_xml("---\ntitle: hello\n...\n\nparagraph",
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
      "<document xmlns=\"http://commonmark.org/xml/1.0\">\n"
      "  <frontmatter xml:space=\"preserve\">title: hello\n</frontmatter>\n"
      "  <paragraph>\n"
      "    <text xml:space=\"preserve\">paragraph</text>\n"
      "  </paragraph>\n"
      "</document>\n",
      CMARK_OPT_DEFAULT);
}

int test_frontmatter_empty() {
  return test_xml("---\n---\n",
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
      "<document xmlns=\"http://commonmark.org/xml/1.0\">\n"
      "  <frontmatter xml:space=\"preserve\"></frontmatter>\n"
      "</document>\n",
      CMARK_OPT_DEFAULT);
}

int test_frontmatter_unclosed() {
  return test_xml("---\ntitle: hello\n",
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
      "<document xmlns=\"http://commonmark.org/xml/1.0\">\n"
      "  <frontmatter xml:space=\"preserve\">title: hello\n</frontmatter>\n"
      "</document>\n",
      CMARK_OPT_DEFAULT);
}

int test_frontmatter_with_blank_lines() {
  return test_xml("---\n\ntitle: hello\n\n---\n",
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
      "<document xmlns=\"http://commonmark.org/xml/1.0\">\n"
      "  <frontmatter xml:space=\"preserve\">\ntitle: hello\n\n</frontmatter>\n"
      "</document>\n",
      CMARK_OPT_DEFAULT);
}

int test_frontmatter_not_first_line() {
  return test_xml("\n---\ntitle: hello\n---\n",
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
      "<document xmlns=\"http://commonmark.org/xml/1.0\">\n"
      "  <thematic_break />\n"
      "  <heading level=\"2\">\n"
      "    <text xml:space=\"preserve\">title: hello</text>\n"
      "  </heading>\n"
      "</document>\n",
      CMARK_OPT_DEFAULT);
}

int test_frontmatter_four_hyphens() {
  return test_xml("----\ntitle: hello\n---\n",
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
      "<document xmlns=\"http://commonmark.org/xml/1.0\">\n"
      "  <thematic_break />\n"
      "  <heading level=\"2\">\n"
      "    <text xml:space=\"preserve\">title: hello</text>\n"
      "  </heading>\n"
      "</document>\n",
      CMARK_OPT_DEFAULT);
}

int test_frontmatter_in_blockquote() {
  return test_xml("> ---\ntitle: hello\n---\n",
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
      "<document xmlns=\"http://commonmark.org/xml/1.0\">\n"
      "  <block_quote>\n"
      "    <thematic_break />\n"
      "  </block_quote>\n"
      "  <heading level=\"2\">\n"
      "    <text xml:space=\"preserve\">title: hello</text>\n"
      "  </heading>\n"
      "</document>\n",
      CMARK_OPT_DEFAULT);
}

int test_frontmatter_indented() {
  return test_xml("  ---\ntitle: hello\n---\n",
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
      "<document xmlns=\"http://commonmark.org/xml/1.0\">\n"
      "  <thematic_break />\n"
      "  <heading level=\"2\">\n"
      "    <text xml:space=\"preserve\">title: hello</text>\n"
      "  </heading>\n"
      "</document>\n",
      CMARK_OPT_DEFAULT);
}

int test_frontmatter_simple_html() {
  return test_html("---\ntitle: hello\n---\n\nparagraph",
      "<p>paragraph</p>\n",
      CMARK_OPT_DEFAULT);
}

int test_frontmatter_empty_html() {
  return test_html("---\n---\n",
      "",
      CMARK_OPT_DEFAULT);
}

int main() {
  CASE(test_frontmatter_simple);
  CASE(test_frontmatter_dots_closer);
  CASE(test_frontmatter_empty);
  CASE(test_frontmatter_unclosed);
  CASE(test_frontmatter_with_blank_lines);
  CASE(test_frontmatter_not_first_line);
  CASE(test_frontmatter_four_hyphens);
  CASE(test_frontmatter_in_blockquote);
  CASE(test_frontmatter_indented);
  CASE(test_frontmatter_simple_html);
  CASE(test_frontmatter_empty_html);
  return 0;
}
