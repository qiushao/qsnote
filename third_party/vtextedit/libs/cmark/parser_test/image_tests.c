#include "cmark.h"
#include "test_utils.h"

#include <stdio.h>
#include <string.h>

static int test_basic_image() {
    const char *markdown = "![Alt text](image.png)\n";
    const char *expected = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                           "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
                           "<document sourcepos=\"1:1-1:22\" xmlns=\"http://commonmark.org/xml/1.0\">\n"
                           "  <paragraph sourcepos=\"1:1-1:22\">\n"
                           "    <image sourcepos=\"1:1-1:22\" destination=\"image.png\" urlpos=\"1:13-1:21\">\n"
                           "      <text sourcepos=\"1:3-1:10\" xml:space=\"preserve\">Alt text</text>\n"
                           "    </image>\n"
                           "  </paragraph>\n"
                           "</document>\n";
    return test_xml(markdown, expected, CMARK_OPT_SOURCEPOS);
}

static int test_image_with_title() {
    const char *markdown = "![Alt text](image.png \"Image title\")\n";
    const char *expected = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                           "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
                           "<document xmlns=\"http://commonmark.org/xml/1.0\">\n"
                           "  <paragraph>\n"
                           "    <image destination=\"image.png\" title=\"Image title\">\n"
                           "      <text xml:space=\"preserve\">Alt text</text>\n"
                           "    </image>\n"
                           "  </paragraph>\n"
                           "</document>\n";
    return test_xml(markdown, expected, CMARK_OPT_DEFAULT);
}

static int test_image_with_width_and_height() {
    const char *markdown = "![Alt text](image.png =800x600)\n";
    const char *expected = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                           "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
                           "<document xmlns=\"http://commonmark.org/xml/1.0\">\n"
                           "  <paragraph>\n"
                           "    <image destination=\"image.png\" width=\"800\" height=\"600\">\n"
                           "      <text xml:space=\"preserve\">Alt text</text>\n"
                           "    </image>\n"
                           "  </paragraph>\n"
                           "</document>\n";
    return test_xml(markdown, expected, CMARK_OPT_DEFAULT);
}

static int test_image_with_width_only() {
    const char *markdown = "![Alt text](image.png =800x)\n";
    const char *expected = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                           "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
                           "<document xmlns=\"http://commonmark.org/xml/1.0\">\n"
                           "  <paragraph>\n"
                           "    <image destination=\"image.png\" width=\"800\">\n"
                           "      <text xml:space=\"preserve\">Alt text</text>\n"
                           "    </image>\n"
                           "  </paragraph>\n"
                           "</document>\n";
    return test_xml(markdown, expected, CMARK_OPT_DEFAULT);
}

static int test_image_with_height_only() {
    const char *markdown = "![Alt text](image.png =x600)\n";
    const char *expected = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                           "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
                           "<document xmlns=\"http://commonmark.org/xml/1.0\">\n"
                           "  <paragraph>\n"
                           "    <image destination=\"image.png\" height=\"600\">\n"
                           "      <text xml:space=\"preserve\">Alt text</text>\n"
                           "    </image>\n"
                           "  </paragraph>\n"
                           "</document>\n";
    return test_xml(markdown, expected, CMARK_OPT_DEFAULT);
}

static int test_image_with_title_and_dimensions() {
    const char *markdown = "![Alt text](image.png \"Image title\" =800x600)\n";
    const char *expected = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                           "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
                           "<document xmlns=\"http://commonmark.org/xml/1.0\">\n"
                           "  <paragraph>\n"
                           "    <image destination=\"image.png\" title=\"Image title\" width=\"800\" height=\"600\">\n"
                           "      <text xml:space=\"preserve\">Alt text</text>\n"
                           "    </image>\n"
                           "  </paragraph>\n"
                           "</document>\n";
    return test_xml(markdown, expected, CMARK_OPT_DEFAULT);
}

static int test_image_with_title_and_width_only() {
    const char *markdown = "![Alt text](image.png \"Image title\" =800x)\n";
    const char *expected = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                           "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
                           "<document xmlns=\"http://commonmark.org/xml/1.0\">\n"
                           "  <paragraph>\n"
                           "    <image destination=\"image.png\" title=\"Image title\" width=\"800\">\n"
                           "      <text xml:space=\"preserve\">Alt text</text>\n"
                           "    </image>\n"
                           "  </paragraph>\n"
                           "</document>\n";
    return test_xml(markdown, expected, CMARK_OPT_DEFAULT);
}

static int test_image_with_title_and_height_only() {
    const char *markdown = "![Alt text](image.png \"Image title\" =x600)\n";
    const char *expected = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                           "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
                           "<document xmlns=\"http://commonmark.org/xml/1.0\">\n"
                           "  <paragraph>\n"
                           "    <image destination=\"image.png\" title=\"Image title\" height=\"600\">\n"
                           "      <text xml:space=\"preserve\">Alt text</text>\n"
                           "    </image>\n"
                           "  </paragraph>\n"
                           "</document>\n";
    return test_xml(markdown, expected, CMARK_OPT_DEFAULT);
}

static int test_image_reference() {
    const char *markdown = "![Alt text][ref]\n\n[ref]: image.png\n";
    const char *expected = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                           "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
                           "<document xmlns=\"http://commonmark.org/xml/1.0\">\n"
                           "  <paragraph>\n"
                           "    <image destination=\"image.png\">\n"
                           "      <text xml:space=\"preserve\">Alt text</text>\n"
                           "    </image>\n"
                           "  </paragraph>\n"
                           "</document>\n";
    return test_xml(markdown, expected, CMARK_OPT_DEFAULT);
}

static int test_image_with_complex_alt_text() {
    const char *markdown = "![Alt *text* with **formatting**](image.png =800x600)\n";
    const char *expected = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                           "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
                           "<document xmlns=\"http://commonmark.org/xml/1.0\">\n"
                           "  <paragraph>\n"
                           "    <image destination=\"image.png\" width=\"800\" height=\"600\">\n"
                           "      <text xml:space=\"preserve\">Alt </text>\n"
                           "      <emph>\n"
                           "        <text xml:space=\"preserve\">text</text>\n"
                           "      </emph>\n"
                           "      <text xml:space=\"preserve\"> with </text>\n"
                           "      <strong>\n"
                           "        <text xml:space=\"preserve\">formatting</text>\n"
                           "      </strong>\n"
                           "    </image>\n"
                           "  </paragraph>\n"
                           "</document>\n";
    return test_xml(markdown, expected, CMARK_OPT_DEFAULT);
}

static int test_image_invalid_size_syntax() {
    // Invalid size syntax should be ignored
    const char *markdown = "![Alt text](image.png =invalid)\n";
    const char *expected = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                           "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
                           "<document xmlns=\"http://commonmark.org/xml/1.0\">\n"
                           "  <paragraph>\n"
                           "    <text xml:space=\"preserve\">![Alt text](image.png =invalid)</text>\n"
                           "  </paragraph>\n"
                           "</document>\n";
    return test_xml(markdown, expected, CMARK_OPT_DEFAULT);
}

// --- source positions -------------------------------------------------------
//
// vnote's editor maps cmark's reported coordinates onto QTextDocument offsets in
// order to place in-place previews and to rewrite image destinations in the raw
// text. Those two operations need, respectively, an exact span for the whole
// construct and an exact span for the *raw* destination as spelled in the input
// (`urlpos`) -- which is not the same string as `destination`, because cmark
// unescapes backslash escapes and entities and strips angle brackets.

// The image span must start at the `!`, not at the `[`; column 1 here.
static int test_image_sourcepos_starts_at_bang() {
    return test_xml(
        "![a](i.png)\n",
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
        "<document sourcepos=\"1:1-1:11\" xmlns=\"http://commonmark.org/xml/1.0\">\n"
        "  <paragraph sourcepos=\"1:1-1:11\">\n"
        "    <image sourcepos=\"1:1-1:11\" destination=\"i.png\" urlpos=\"1:6-1:10\">\n"
        "      <text sourcepos=\"1:3-1:3\" xml:space=\"preserve\">a</text>\n"
        "    </image>\n"
        "  </paragraph>\n"
        "</document>\n",
        CMARK_OPT_SOURCEPOS);
}

// A newline inside the alt text: the span must cover both lines.
static int test_image_multiline_alt_sourcepos() {
    return test_xml(
        "![a\nb](i.png)\n",
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
        "<document sourcepos=\"1:1-2:9\" xmlns=\"http://commonmark.org/xml/1.0\">\n"
        "  <paragraph sourcepos=\"1:1-2:9\">\n"
        "    <image sourcepos=\"1:1-2:9\" destination=\"i.png\" urlpos=\"2:4-2:8\">\n"
        "      <text sourcepos=\"1:3-1:3\" xml:space=\"preserve\">a</text>\n"
        "      <softbreak />\n"
        "      <text sourcepos=\"2:1-2:1\" xml:space=\"preserve\">b</text>\n"
        "    </image>\n"
        "  </paragraph>\n"
        "</document>\n",
        CMARK_OPT_SOURCEPOS);
}

// A newline after `(`: consumed by a scanner rather than by parse_inline, so the
// subject's line counter has to be caught up explicitly.
static int test_image_multiline_destination_sourcepos() {
    return test_xml(
        "![a](\ni.png)\n",
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
        "<document sourcepos=\"1:1-2:6\" xmlns=\"http://commonmark.org/xml/1.0\">\n"
        "  <paragraph sourcepos=\"1:1-2:6\">\n"
        "    <image sourcepos=\"1:1-2:6\" destination=\"i.png\" urlpos=\"2:1-2:5\">\n"
        "      <text sourcepos=\"1:3-1:3\" xml:space=\"preserve\">a</text>\n"
        "    </image>\n"
        "  </paragraph>\n"
        "</document>\n",
        CMARK_OPT_SOURCEPOS);
}

// A single-image case: `md` must render to a document holding exactly one
// paragraph whose only child is `img`, and whose span equals the image's.
typedef struct {
    const char *md;
    const char *img;
} image_case;

// Returns 1 only if every case passes, matching test_xml()'s convention.
static int run_image_cases(const image_case *cases, size_t n) {
    size_t i;
    int passed = 1;

    for (i = 0; i < n; ++i) {
        char expected[1024];
        char paragraph[64];
        const char *sourcepos = strstr(cases[i].img, "sourcepos=\"") + 11;
        const char *end = strchr(sourcepos, '"');
        // snprintf's size argument counts the terminator, so this copies
        // exactly the `L:C-L:C` substring.
        snprintf(paragraph, (size_t)(end - sourcepos) + 1, "%s", sourcepos);
        snprintf(expected, sizeof(expected),
                 "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                 "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
                 "<document sourcepos=\"%s\" xmlns=\"http://commonmark.org/xml/1.0\">\n"
                 "  <paragraph sourcepos=\"%s\">\n"
                 "%s"
                 "  </paragraph>\n"
                 "</document>\n",
                 paragraph, paragraph, cases[i].img);
        if (!test_xml(cases[i].md, expected, CMARK_OPT_SOURCEPOS)) {
            passed = 0;
        }
    }

    return passed;
}

// One void-image case per destination spelling that makes `destination` differ
// from the raw source text, plus the grammar corners a hand-written scanner
// would get wrong (a title containing `](`, balanced and escaped parens in the
// destination, and each of the three title delimiters).
static int test_image_urlpos_variants() {
    static const image_case cases[] = {
        {"![](a\\_b.png)\n",
         "    <image sourcepos=\"1:1-1:13\" destination=\"a_b.png\" urlpos=\"1:5-1:12\" />\n"},
        {"![](<a b.png>)\n",
         // The angle brackets are part of the raw span; rewriting must preserve them.
         "    <image sourcepos=\"1:1-1:14\" destination=\"a b.png\" urlpos=\"1:5-1:13\" />\n"},
        {"![](a&amp;b.png)\n",
         "    <image sourcepos=\"1:1-1:16\" destination=\"a&amp;b.png\" urlpos=\"1:5-1:15\" />\n"},
        {"![](a%20b.png)\n",
         "    <image sourcepos=\"1:1-1:14\" destination=\"a%20b.png\" urlpos=\"1:5-1:13\" />\n"},
        {"![](i.png \"a](b\")\n",
         "    <image sourcepos=\"1:1-1:17\" destination=\"i.png\" title=\"a](b\" urlpos=\"1:5-1:9\" />\n"},
        {"![](a(b)c.png)\n",
         "    <image sourcepos=\"1:1-1:14\" destination=\"a(b)c.png\" urlpos=\"1:5-1:13\" />\n"},
        {"![](a\\(b.png)\n",
         "    <image sourcepos=\"1:1-1:13\" destination=\"a(b.png\" urlpos=\"1:5-1:12\" />\n"},
        {"![](i.png 'ti')\n",
         "    <image sourcepos=\"1:1-1:15\" destination=\"i.png\" title=\"ti\" urlpos=\"1:5-1:9\" />\n"},
        {"![](i.png (ti))\n",
         "    <image sourcepos=\"1:1-1:15\" destination=\"i.png\" title=\"ti\" urlpos=\"1:5-1:9\" />\n"},
    };
    return run_image_cases(cases, sizeof(cases) / sizeof(cases[0]));
}

// An empty inline destination has no bytes to point at, so it gets no urlpos --
// rather than a reversed, inclusive-end range. `<>` does have bytes, and keeps
// one, because rewriting has to preserve the angle brackets.
static int test_image_empty_destination_urlpos() {
    static const image_case cases[] = {
        {"![a]()\n",
         "    <image sourcepos=\"1:1-1:6\" destination=\"\">\n"
         "      <text sourcepos=\"1:3-1:3\" xml:space=\"preserve\">a</text>\n"
         "    </image>\n"},
        {"![](\n)\n",
         "    <image sourcepos=\"1:1-2:1\" destination=\"\" />\n"},
        {"![](<>)\n",
         "    <image sourcepos=\"1:1-1:7\" destination=\"\" urlpos=\"1:5-1:6\" />\n"},
    };
    return run_image_cases(cases, sizeof(cases) / sizeof(cases[0]));
}

// The `=WxH` suffix is never part of the destination or of its raw span, and it
// only applies when separated by whitespace.
static int test_image_size_urlpos() {
    static const image_case cases[] = {
        {"![](a.png =500x)\n",
         "    <image sourcepos=\"1:1-1:16\" destination=\"a.png\" width=\"500\" urlpos=\"1:5-1:9\" />\n"},
        {"![](a.png =500x300)\n",
         "    <image sourcepos=\"1:1-1:19\" destination=\"a.png\" width=\"500\" height=\"300\" urlpos=\"1:5-1:9\" />\n"},
        {"![](a.png =x300)\n",
         "    <image sourcepos=\"1:1-1:16\" destination=\"a.png\" height=\"300\" urlpos=\"1:5-1:9\" />\n"},
        {"![](a.png \"t\" =500x)\n",
         "    <image sourcepos=\"1:1-1:20\" destination=\"a.png\" title=\"t\" width=\"500\" urlpos=\"1:5-1:9\" />\n"},
        // No separating space: the whole thing is the destination.
        {"![](a.png=500x)\n",
         "    <image sourcepos=\"1:1-1:15\" destination=\"a.png=500x\" urlpos=\"1:5-1:14\" />\n"},
    };
    return run_image_cases(cases, sizeof(cases) / sizeof(cases[0]));
}

// A reference-style image has no inline destination, hence no urlpos at all.
static int test_image_reference_has_no_urlpos() {
    return test_xml(
        "![a][r]\n\n[r]: p.png\n",
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
        "<document sourcepos=\"1:1-3:10\" xmlns=\"http://commonmark.org/xml/1.0\">\n"
        "  <paragraph sourcepos=\"1:1-1:7\">\n"
        "    <image sourcepos=\"1:1-1:7\" destination=\"p.png\">\n"
        "      <text sourcepos=\"1:3-1:3\" xml:space=\"preserve\">a</text>\n"
        "    </image>\n"
        "  </paragraph>\n"
        "</document>\n",
        CMARK_OPT_SOURCEPOS);
}

// Inside a block quote the column must include the stripped `> ` prefix.
static int test_image_in_block_quote_urlpos() {
    return test_xml(
        "> ![q](qq.png)\n",
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
        "<document sourcepos=\"1:1-1:14\" xmlns=\"http://commonmark.org/xml/1.0\">\n"
        "  <block_quote sourcepos=\"1:1-1:14\">\n"
        "    <paragraph sourcepos=\"1:3-1:14\">\n"
        "      <image sourcepos=\"1:3-1:14\" destination=\"qq.png\" urlpos=\"1:8-1:13\">\n"
        "        <text sourcepos=\"1:5-1:5\" xml:space=\"preserve\">q</text>\n"
        "      </image>\n"
        "    </paragraph>\n"
        "  </block_quote>\n"
        "</document>\n",
        CMARK_OPT_SOURCEPOS);
}

// The two corrections above have to compose: a destination that wraps onto a
// continuation line, INSIDE a container that strips a prefix from that line.
//
// The newline is consumed by a scanner rather than by parse_inline(), so the
// continuation column is derived here rather than by handle_newline(), and it
// must still carry block_offset. Getting only one of the two right leaves the
// span short by the prefix width, which a consumer mapping these coordinates
// back onto the original text has no way to detect. Note the image's span
// matches its enclosing paragraph's end in both cases -- that is the invariant.
static int test_image_multiline_destination_in_container() {
    int passed = 1;

    if (!test_xml("> ![alt](\n> img.png)\n",
                  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                  "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
                  "<document sourcepos=\"1:1-2:10\" xmlns=\"http://commonmark.org/xml/1.0\">\n"
                  "  <block_quote sourcepos=\"1:1-2:10\">\n"
                  "    <paragraph sourcepos=\"1:3-2:10\">\n"
                  "      <image sourcepos=\"1:3-2:10\" destination=\"img.png\" urlpos=\"2:3-2:9\">\n"
                  "        <text sourcepos=\"1:5-1:7\" xml:space=\"preserve\">alt</text>\n"
                  "      </image>\n"
                  "    </paragraph>\n"
                  "  </block_quote>\n"
                  "</document>\n",
                  CMARK_OPT_SOURCEPOS)) {
        passed = 0;
    }

    // Same shape in a list item, where the stripped prefix is indentation
    // rather than a marker character.
    if (!test_xml("- ![alt](\n  img.png)\n",
                  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                  "<!DOCTYPE document SYSTEM \"CommonMark.dtd\">\n"
                  "<document sourcepos=\"1:1-2:10\" xmlns=\"http://commonmark.org/xml/1.0\">\n"
                  "  <list sourcepos=\"1:1-2:10\" type=\"bullet\" tight=\"true\">\n"
                  "    <item sourcepos=\"1:1-2:10\">\n"
                  "      <paragraph sourcepos=\"1:3-2:10\">\n"
                  "        <image sourcepos=\"1:3-2:10\" destination=\"img.png\" urlpos=\"2:3-2:9\">\n"
                  "          <text sourcepos=\"1:5-1:7\" xml:space=\"preserve\">alt</text>\n"
                  "        </image>\n"
                  "      </paragraph>\n"
                  "    </item>\n"
                  "  </list>\n"
                  "</document>\n",
                  CMARK_OPT_SOURCEPOS)) {
        passed = 0;
    }

    return passed;
}

int main() {
    CASE(test_basic_image);
    CASE(test_image_with_title);
    CASE(test_image_with_width_and_height);
    CASE(test_image_with_width_only);
    CASE(test_image_with_height_only);
    CASE(test_image_with_title_and_dimensions);
    CASE(test_image_with_title_and_width_only);
    CASE(test_image_with_title_and_height_only);
    CASE(test_image_reference);
    CASE(test_image_with_complex_alt_text);
    CASE(test_image_invalid_size_syntax);
    CASE(test_image_sourcepos_starts_at_bang);
    CASE(test_image_multiline_alt_sourcepos);
    CASE(test_image_multiline_destination_sourcepos);
    CASE(test_image_urlpos_variants);
    CASE(test_image_empty_destination_urlpos);
    CASE(test_image_size_urlpos);
    CASE(test_image_reference_has_no_urlpos);
    CASE(test_image_in_block_quote_urlpos);
    CASE(test_image_multiline_destination_in_container);
    return 0;
}
