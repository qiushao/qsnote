#include <cmark.h>

#include "test_utils.h"

int test_wikilink_simple() {
  return test_xml("[[Page]]",
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
      "<document xmlns=\"http://commonmark.org/xml/1.0\">\n"
      "  <paragraph>\n"
      "    <wikilink destination=\"Page\">\n"
      "      <text xml:space=\"preserve\">Page</text>\n"
      "    </wikilink>\n"
      "  </paragraph>\n"
      "</document>\n",
      CMARK_OPT_DEFAULT);
}

int test_wikilink_display_text() {
  return test_xml("[[Page|Display]]",
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
      "<document xmlns=\"http://commonmark.org/xml/1.0\">\n"
      "  <paragraph>\n"
      "    <wikilink destination=\"Page\">\n"
      "      <text xml:space=\"preserve\">Display</text>\n"
      "    </wikilink>\n"
      "  </paragraph>\n"
      "</document>\n",
      CMARK_OPT_DEFAULT);
}

int test_wikilink_heading() {
  return test_xml("[[Page#Heading]]",
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
      "<document xmlns=\"http://commonmark.org/xml/1.0\">\n"
      "  <paragraph>\n"
      "    <wikilink destination=\"Page#Heading\">\n"
      "      <text xml:space=\"preserve\">Page#Heading</text>\n"
      "    </wikilink>\n"
      "  </paragraph>\n"
      "</document>\n",
      CMARK_OPT_DEFAULT);
}

int test_wikilink_heading_display() {
  return test_xml("[[Page#H|Custom]]",
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
      "<document xmlns=\"http://commonmark.org/xml/1.0\">\n"
      "  <paragraph>\n"
      "    <wikilink destination=\"Page#H\">\n"
      "      <text xml:space=\"preserve\">Custom</text>\n"
      "    </wikilink>\n"
      "  </paragraph>\n"
      "</document>\n",
      CMARK_OPT_DEFAULT);
}

int test_wikilink_multiple() {
  return test_xml("[[A]] and [[B]]",
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
      "<document xmlns=\"http://commonmark.org/xml/1.0\">\n"
      "  <paragraph>\n"
      "    <wikilink destination=\"A\">\n"
      "      <text xml:space=\"preserve\">A</text>\n"
      "    </wikilink>\n"
      "    <text xml:space=\"preserve\"> and </text>\n"
      "    <wikilink destination=\"B\">\n"
      "      <text xml:space=\"preserve\">B</text>\n"
      "    </wikilink>\n"
      "  </paragraph>\n"
      "</document>\n",
      CMARK_OPT_DEFAULT);
}

int test_wikilink_empty() {
  return test_xml("[[]]",
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
      "<document xmlns=\"http://commonmark.org/xml/1.0\">\n"
      "  <paragraph>\n"
      "    <text xml:space=\"preserve\">[[]]</text>\n"
      "  </paragraph>\n"
      "</document>\n",
      CMARK_OPT_DEFAULT);
}

int test_wikilink_unclosed() {
  return test_xml("[[unclosed",
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
      "<document xmlns=\"http://commonmark.org/xml/1.0\">\n"
      "  <paragraph>\n"
      "    <text xml:space=\"preserve\">[[unclosed</text>\n"
      "  </paragraph>\n"
      "</document>\n",
      CMARK_OPT_DEFAULT);
}

int test_wikilink_simple_html() {
  return test_html("[[Page]]",
      "<p><a class=\"wikilink\" href=\"Page\">Page</a></p>\n",
      CMARK_OPT_DEFAULT);
}

int test_wikilink_display_html() {
  return test_html("[[Page|Text]]",
      "<p><a class=\"wikilink\" href=\"Page\">Text</a></p>\n",
      CMARK_OPT_DEFAULT);
}

int test_wikilink_heading_html() {
  return test_html("[[Page#H]]",
      "<p><a class=\"wikilink\" href=\"Page#H\">Page#H</a></p>\n",
      CMARK_OPT_DEFAULT);
}

int test_wikilink_with_text() {
  return test_xml("before [[Page]] after",
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
      "<document xmlns=\"http://commonmark.org/xml/1.0\">\n"
      "  <paragraph>\n"
      "    <text xml:space=\"preserve\">before </text>\n"
      "    <wikilink destination=\"Page\">\n"
      "      <text xml:space=\"preserve\">Page</text>\n"
      "    </wikilink>\n"
      "    <text xml:space=\"preserve\"> after</text>\n"
      "  </paragraph>\n"
      "</document>\n",
      CMARK_OPT_DEFAULT);
}

int test_wikilink_multiple_pipes() {
  return test_xml("[[a|b|c]]",
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
      "<document xmlns=\"http://commonmark.org/xml/1.0\">\n"
      "  <paragraph>\n"
      "    <wikilink destination=\"a\">\n"
      "      <text xml:space=\"preserve\">b|c</text>\n"
      "    </wikilink>\n"
      "  </paragraph>\n"
      "</document>\n",
      CMARK_OPT_DEFAULT);
}

int main() {
  CASE(test_wikilink_simple);
  CASE(test_wikilink_display_text);
  CASE(test_wikilink_heading);
  CASE(test_wikilink_heading_display);
  CASE(test_wikilink_multiple);
  CASE(test_wikilink_empty);
  CASE(test_wikilink_unclosed);
  CASE(test_wikilink_simple_html);
  CASE(test_wikilink_display_html);
  CASE(test_wikilink_heading_html);
  CASE(test_wikilink_with_text);
  CASE(test_wikilink_multiple_pipes);
  return 0;
}
