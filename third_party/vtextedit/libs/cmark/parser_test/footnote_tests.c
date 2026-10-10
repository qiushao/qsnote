#include <cmark.h>

#include "test_utils.h"

int test_footnote_def_simple() {
  return test_xml("[^a]: Simple definition",
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
      "<document xmlns=\"http://commonmark.org/xml/1.0\">\n"
      "  <footnote_definition label=\"a\">\n"
      "    <paragraph>\n"
      "      <text xml:space=\"preserve\">Simple definition</text>\n"
      "    </paragraph>\n"
      "  </footnote_definition>\n"
      "</document>\n",
      CMARK_OPT_DEFAULT);
}

int test_footnote_def_multiline() {
  return test_xml("[^a]: First paragraph\n\n      Second paragraph\n\nNormal text",
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
      "<document xmlns=\"http://commonmark.org/xml/1.0\">\n"
      "  <footnote_definition label=\"a\">\n"
      "    <paragraph>\n"
      "      <text xml:space=\"preserve\">First paragraph</text>\n"
      "    </paragraph>\n"
      "    <paragraph>\n"
      "      <text xml:space=\"preserve\">Second paragraph</text>\n"
      "    </paragraph>\n"
      "  </footnote_definition>\n"
      "  <paragraph>\n"
      "    <text xml:space=\"preserve\">Normal text</text>\n"
      "  </paragraph>\n"
      "</document>\n",
      CMARK_OPT_DEFAULT);
}

int test_footnote_def_empty_label() {
  return test_xml("[^]: text",
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
      "<document xmlns=\"http://commonmark.org/xml/1.0\" />\n",
      CMARK_OPT_DEFAULT);
}

int test_footnote_def_not_started_indented() {
  return test_xml("    [^a]: indented too much",
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
      "<document xmlns=\"http://commonmark.org/xml/1.0\">\n"
      "  <code_block xml:space=\"preserve\">[^a]: indented too much\n</code_block>\n"
      "</document>\n",
      CMARK_OPT_DEFAULT);
}

int test_footnote_ref_simple() {
  return test_xml("Text[^a]\n\n[^a]: Definition",
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
      "<document xmlns=\"http://commonmark.org/xml/1.0\">\n"
      "  <paragraph>\n"
      "    <text xml:space=\"preserve\">Text</text>\n"
      "    <footnote_reference label=\"a\" number=\"1\" />\n"
      "  </paragraph>\n"
      "  <footnote_definition label=\"a\">\n"
      "    <paragraph>\n"
      "      <text xml:space=\"preserve\">Definition</text>\n"
      "    </paragraph>\n"
      "  </footnote_definition>\n"
      "</document>\n",
      CMARK_OPT_DEFAULT);
}

int test_footnote_ref_unresolved() {
  return test_xml("Text[^missing]",
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
      "<document xmlns=\"http://commonmark.org/xml/1.0\">\n"
      "  <paragraph>\n"
      "    <text xml:space=\"preserve\">Text[^missing]</text>\n"
      "  </paragraph>\n"
      "</document>\n",
      CMARK_OPT_DEFAULT);
}

int test_footnote_ref_forward() {
  return test_xml("[^a] before def\n\n[^a]: Def",
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
      "<document xmlns=\"http://commonmark.org/xml/1.0\">\n"
      "  <paragraph>\n"
      "    <footnote_reference label=\"a\" number=\"1\" />\n"
      "    <text xml:space=\"preserve\"> before def</text>\n"
      "  </paragraph>\n"
      "  <footnote_definition label=\"a\">\n"
      "    <paragraph>\n"
      "      <text xml:space=\"preserve\">Def</text>\n"
      "    </paragraph>\n"
      "  </footnote_definition>\n"
      "</document>\n",
      CMARK_OPT_DEFAULT);
}

int test_footnote_ref_multiple_same() {
  return test_xml("[^a] and [^a]\n\n[^a]: Def",
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
      "<document xmlns=\"http://commonmark.org/xml/1.0\">\n"
      "  <paragraph>\n"
      "    <footnote_reference label=\"a\" number=\"1\" />\n"
      "    <text xml:space=\"preserve\"> and </text>\n"
      "    <footnote_reference label=\"a\" number=\"1\" />\n"
      "  </paragraph>\n"
      "  <footnote_definition label=\"a\">\n"
      "    <paragraph>\n"
      "      <text xml:space=\"preserve\">Def</text>\n"
      "    </paragraph>\n"
      "  </footnote_definition>\n"
      "</document>\n",
      CMARK_OPT_DEFAULT);
}

int test_inline_note_simple() {
  return test_xml("Text ^[inline note]",
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
      "<document xmlns=\"http://commonmark.org/xml/1.0\">\n"
      "  <paragraph>\n"
      "    <text xml:space=\"preserve\">Text </text>\n"
      "    <inline_footnote number=\"1\">\n"
      "      <text xml:space=\"preserve\">inline note</text>\n"
      "    </inline_footnote>\n"
      "  </paragraph>\n"
      "</document>\n",
      CMARK_OPT_DEFAULT);
}

int test_inline_note_with_markup() {
  return test_xml("^[**bold** text]",
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
      "<document xmlns=\"http://commonmark.org/xml/1.0\">\n"
      "  <paragraph>\n"
      "    <inline_footnote number=\"1\">\n"
      "      <strong>\n"
      "        <text xml:space=\"preserve\">bold</text>\n"
      "      </strong>\n"
      "      <text xml:space=\"preserve\"> text</text>\n"
      "    </inline_footnote>\n"
      "  </paragraph>\n"
      "</document>\n",
      CMARK_OPT_DEFAULT);
}

int test_inline_note_unclosed() {
  return test_xml("^[unclosed",
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
      "<document xmlns=\"http://commonmark.org/xml/1.0\">\n"
      "  <paragraph>\n"
      "    <text xml:space=\"preserve\">^[unclosed</text>\n"
      "  </paragraph>\n"
      "</document>\n",
      CMARK_OPT_DEFAULT);
}

int test_inline_note_empty() {
  return test_xml("^[]",
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
      "<document xmlns=\"http://commonmark.org/xml/1.0\">\n"
      "  <paragraph>\n"
      "    <inline_footnote number=\"1\" />\n"
      "  </paragraph>\n"
      "</document>\n",
      CMARK_OPT_DEFAULT);
}

int test_footnote_numbering_order() {
  return test_xml("[^b] and [^a]\n\n[^a]: Alpha\n\n[^b]: Beta",
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
      "<document xmlns=\"http://commonmark.org/xml/1.0\">\n"
      "  <paragraph>\n"
      "    <footnote_reference label=\"b\" number=\"1\" />\n"
      "    <text xml:space=\"preserve\"> and </text>\n"
      "    <footnote_reference label=\"a\" number=\"2\" />\n"
      "  </paragraph>\n"
      "  <footnote_definition label=\"a\">\n"
      "    <paragraph>\n"
      "      <text xml:space=\"preserve\">Alpha</text>\n"
      "    </paragraph>\n"
      "  </footnote_definition>\n"
      "  <footnote_definition label=\"b\">\n"
      "    <paragraph>\n"
      "      <text xml:space=\"preserve\">Beta</text>\n"
      "    </paragraph>\n"
      "  </footnote_definition>\n"
      "</document>\n",
      CMARK_OPT_DEFAULT);
}

int test_footnote_interleaved() {
  return test_xml("[^a] ^[inline] [^b]\n\n[^a]: Alpha\n\n[^b]: Beta",
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
      "<document xmlns=\"http://commonmark.org/xml/1.0\">\n"
      "  <paragraph>\n"
      "    <footnote_reference label=\"a\" number=\"1\" />\n"
      "    <text xml:space=\"preserve\"> </text>\n"
      "    <inline_footnote number=\"2\">\n"
      "      <text xml:space=\"preserve\">inline</text>\n"
      "    </inline_footnote>\n"
      "    <text xml:space=\"preserve\"> </text>\n"
      "    <footnote_reference label=\"b\" number=\"3\" />\n"
      "  </paragraph>\n"
      "  <footnote_definition label=\"a\">\n"
      "    <paragraph>\n"
      "      <text xml:space=\"preserve\">Alpha</text>\n"
      "    </paragraph>\n"
      "  </footnote_definition>\n"
      "  <footnote_definition label=\"b\">\n"
      "    <paragraph>\n"
      "      <text xml:space=\"preserve\">Beta</text>\n"
      "    </paragraph>\n"
      "  </footnote_definition>\n"
      "</document>\n",
      CMARK_OPT_DEFAULT);
}

int main() {
  CASE(test_footnote_def_simple);
  CASE(test_footnote_def_multiline);
  CASE(test_footnote_def_empty_label);
  CASE(test_footnote_def_not_started_indented);
  CASE(test_footnote_ref_simple);
  CASE(test_footnote_ref_unresolved);
  CASE(test_footnote_ref_forward);
  CASE(test_footnote_ref_multiple_same);
  CASE(test_inline_note_simple);
  CASE(test_inline_note_with_markup);
  CASE(test_inline_note_unclosed);
  CASE(test_inline_note_empty);
  CASE(test_footnote_numbering_order);
  CASE(test_footnote_interleaved);
  return 0;
}
