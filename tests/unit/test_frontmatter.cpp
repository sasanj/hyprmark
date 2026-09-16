#include "render/Frontmatter.hpp"
#include "render/MarkdownRenderer.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#ifndef HYPRMARK_FIXTURES_DIR
#error "HYPRMARK_FIXTURES_DIR must be defined at compile time"
#endif

namespace {
    std::string fixture(const std::string& rel) {
        return std::string(HYPRMARK_FIXTURES_DIR) + "/" + rel;
    }

    constexpr const char* kBom = "\xEF\xBB\xBF";
}

// ---------------------------------------------------------------------------
// split(): detection and stripping. Always compiled, no yaml-cpp required.
// ---------------------------------------------------------------------------

TEST(FrontmatterSplitTest, StripsBasicBlock) {
    const auto fm = Frontmatter::split("---\ntitle: Hello\ntags: [a, b]\n---\n# Heading\n");
    ASSERT_TRUE(fm.found);
    EXPECT_EQ(fm.yaml, "title: Hello\ntags: [a, b]\n");
    EXPECT_EQ(fm.body, "# Heading\n");
}

TEST(FrontmatterSplitTest, NoFrontmatterLeavesBodyUntouched) {
    const std::string md = "# Heading\n\nBody.\n";
    const auto        fm = Frontmatter::split(md);
    EXPECT_FALSE(fm.found);
    EXPECT_EQ(fm.body, md);
    EXPECT_TRUE(fm.yaml.empty());
}

TEST(FrontmatterSplitTest, DelimiterNotOnFirstLineIsNotFrontmatter) {
    const auto fm = Frontmatter::split("\n---\ntitle: x\n---\n# H\n");
    EXPECT_FALSE(fm.found);
}

TEST(FrontmatterSplitTest, IndentedDelimiterIsNotFrontmatter) {
    const auto fm = Frontmatter::split("  ---\ntitle: x\n---\n# H\n");
    EXPECT_FALSE(fm.found);
}

TEST(FrontmatterSplitTest, MissingClosingDelimiterIsNotFrontmatter) {
    const std::string md = "---\ntitle: x\n# H\n";
    const auto        fm = Frontmatter::split(md);
    EXPECT_FALSE(fm.found);
    EXPECT_EQ(fm.body, md);
}

TEST(FrontmatterSplitTest, FourDashesIsNotADelimiter) {
    const auto fm = Frontmatter::split("----\ntitle: x\n----\n# H\n");
    EXPECT_FALSE(fm.found);
}

TEST(FrontmatterSplitTest, DelimiterWithTrailingTextIsNotFrontmatter) {
    const auto fm = Frontmatter::split("--- foo\ntitle: x\n---\n# H\n");
    EXPECT_FALSE(fm.found);
}

TEST(FrontmatterSplitTest, EmptyBlock) {
    const auto fm = Frontmatter::split("---\n---\n# H\n");
    ASSERT_TRUE(fm.found);
    EXPECT_TRUE(fm.yaml.empty());
    EXPECT_EQ(fm.body, "# H\n");
}

TEST(FrontmatterSplitTest, EmptyBlockNoTrailingNewline) {
    const auto fm = Frontmatter::split("---\n---");
    ASSERT_TRUE(fm.found);
    EXPECT_TRUE(fm.yaml.empty());
    EXPECT_TRUE(fm.body.empty());
}

TEST(FrontmatterSplitTest, ClosingDelimiterAtEofWithoutNewline) {
    const auto fm = Frontmatter::split("---\ntitle: x\n---");
    ASSERT_TRUE(fm.found);
    EXPECT_EQ(fm.yaml, "title: x\n");
    EXPECT_TRUE(fm.body.empty());
}

TEST(FrontmatterSplitTest, FrontmatterOnlyNoBody) {
    const auto fm = Frontmatter::split("---\ntitle: x\n---\n");
    ASSERT_TRUE(fm.found);
    EXPECT_TRUE(fm.body.empty());
}

TEST(FrontmatterSplitTest, CrlfLineEndings) {
    const auto fm = Frontmatter::split("---\r\ntitle: x\r\n---\r\n# H\r\n");
    ASSERT_TRUE(fm.found);
    EXPECT_EQ(fm.yaml, "title: x\r\n");
    EXPECT_EQ(fm.body, "# H\r\n");
}

TEST(FrontmatterSplitTest, Utf8BomIsDropped) {
    const auto fm = Frontmatter::split(std::string(kBom) + "---\ntitle: x\n---\n# H\n");
    ASSERT_TRUE(fm.found);
    EXPECT_EQ(fm.body, "# H\n");
}

TEST(FrontmatterSplitTest, Utf8BomWithoutFrontmatterKeepsInput) {
    const std::string md = std::string(kBom) + "# H\n";
    const auto        fm = Frontmatter::split(md);
    EXPECT_FALSE(fm.found);
    EXPECT_EQ(fm.body, md);
}

TEST(FrontmatterSplitTest, TrailingWhitespaceAfterDelimiter) {
    const auto fm = Frontmatter::split("---   \ntitle: x\n---\t\n# H\n");
    ASSERT_TRUE(fm.found);
    EXPECT_EQ(fm.yaml, "title: x\n");
    EXPECT_EQ(fm.body, "# H\n");
}

TEST(FrontmatterSplitTest, BlankLinesInsideYamlPreserved) {
    const auto fm = Frontmatter::split("---\ntitle: x\n\nlist:\n  - a\n---\n# H\n");
    ASSERT_TRUE(fm.found);
    EXPECT_EQ(fm.yaml, "title: x\n\nlist:\n  - a\n");
}

TEST(FrontmatterSplitTest, ThematicBreakAfterBlockStays) {
    const auto fm = Frontmatter::split("---\ntitle: x\n---\n\n---\n\ntext\n");
    ASSERT_TRUE(fm.found);
    EXPECT_EQ(fm.body, "\n---\n\ntext\n");
}

TEST(FrontmatterSplitTest, FirstClosingDashesWins) {
    const auto fm = Frontmatter::split("---\nkey: a\n---\nkey2: b\n---\n");
    ASSERT_TRUE(fm.found);
    EXPECT_EQ(fm.yaml, "key: a\n");
    EXPECT_EQ(fm.body, "key2: b\n---\n");
}

// ---------------------------------------------------------------------------
// renderPanel(): YAML -> collapsed <details>. Compiled only with the flag.
// ---------------------------------------------------------------------------

#ifdef HYPRMARK_PARSE_FRONTMATTER

TEST(FrontmatterPanelTest, RendersScalars) {
    const auto p = Frontmatter::renderPanel("title: Hello\nauthor: sasan\n");
    EXPECT_NE(p.html.find("<details class=\"hyprmark-meta\">"), std::string::npos);
    EXPECT_NE(p.html.find("<summary>Metadata</summary>"), std::string::npos);
    EXPECT_NE(p.html.find("<dt>title</dt><dd>Hello</dd>"), std::string::npos);
    EXPECT_NE(p.html.find("<dt>author</dt><dd>sasan</dd>"), std::string::npos);
    EXPECT_FALSE(p.isOkf);
}

TEST(FrontmatterPanelTest, OkfBadgeWhenTopLevelTypeIsScalar) {
    const auto p = Frontmatter::renderPanel("type: table\ntitle: T\n");
    EXPECT_TRUE(p.isOkf);
    EXPECT_NE(p.html.find("hyprmark-meta-badge"), std::string::npos);
    EXPECT_NE(p.html.find(">OKF<"), std::string::npos);
    EXPECT_NE(p.html.find("Open Knowledge Format"), std::string::npos);
}

TEST(FrontmatterPanelTest, NoBadgeWithoutType) {
    const auto p = Frontmatter::renderPanel("title: T\n");
    EXPECT_FALSE(p.isOkf);
    EXPECT_EQ(p.html.find("hyprmark-meta-badge"), std::string::npos);
}

TEST(FrontmatterPanelTest, NonScalarTypeIsNotOkf) {
    const auto p = Frontmatter::renderPanel("type: [a, b]\n");
    EXPECT_FALSE(p.isOkf);
}

TEST(FrontmatterPanelTest, TagsRenderAsChips) {
    const auto p = Frontmatter::renderPanel("tags: [sales, crm]\n");
    EXPECT_NE(p.html.find("<dd class=\"hyprmark-tags\">"), std::string::npos);
    EXPECT_NE(p.html.find("<span class=\"hyprmark-tag\">sales</span>"), std::string::npos);
    EXPECT_NE(p.html.find("<span class=\"hyprmark-tag\">crm</span>"), std::string::npos);
}

TEST(FrontmatterPanelTest, ScalarTagsArePlain) {
    const auto p = Frontmatter::renderPanel("tags: solo\n");
    EXPECT_NE(p.html.find("<dd>solo</dd>"), std::string::npos);
    EXPECT_EQ(p.html.find("hyprmark-tag\">"), std::string::npos);
}

TEST(FrontmatterPanelTest, EmptyTagsSequence) {
    const auto p = Frontmatter::renderPanel("tags: []\n");
    EXPECT_NE(p.html.find("<dd class=\"hyprmark-tags\"></dd>"), std::string::npos);
}

TEST(FrontmatterPanelTest, SequenceOfScalars) {
    const auto p = Frontmatter::renderPanel("aliases: [a, b]\n");
    EXPECT_NE(p.html.find("<ul class=\"hyprmark-meta-seq\"><li>a</li><li>b</li></ul>"), std::string::npos);
}

TEST(FrontmatterPanelTest, NestedMap) {
    const auto p = Frontmatter::renderPanel("generated:\n  by: agent\n  at: now\n");
    EXPECT_NE(p.html.find("<dt>generated</dt>"), std::string::npos);
    EXPECT_NE(p.html.find("<dt>by</dt><dd>agent</dd>"), std::string::npos);
    EXPECT_NE(p.html.find("<dt>at</dt><dd>now</dd>"), std::string::npos);
}

TEST(FrontmatterPanelTest, SequenceOfMaps) {
    const auto p = Frontmatter::renderPanel("verified:\n  - by: human:sasan\n    at: yesterday\n");
    EXPECT_NE(p.html.find("<ul class=\"hyprmark-meta-seq\">"), std::string::npos);
    EXPECT_NE(p.html.find("<dt>by</dt><dd>human:sasan</dd>"), std::string::npos);
    EXPECT_NE(p.html.find("<dt>at</dt><dd>yesterday</dd>"), std::string::npos);
}

TEST(FrontmatterPanelTest, DeepNesting) {
    const auto p = Frontmatter::renderPanel("a:\n  - b:\n      - c: deep\n");
    EXPECT_NE(p.html.find("deep"), std::string::npos);
}

TEST(FrontmatterPanelTest, NullValueRendersEmpty) {
    const auto p = Frontmatter::renderPanel("empty:\n");
    EXPECT_NE(p.html.find("<dt>empty</dt><dd></dd>"), std::string::npos);
}

TEST(FrontmatterPanelTest, BooleansAndNumbers) {
    const auto p = Frontmatter::renderPanel("draft: true\ncount: 3\nratio: 1.5\n");
    EXPECT_NE(p.html.find("<dd>true</dd>"), std::string::npos);
    EXPECT_NE(p.html.find("<dd>3</dd>"), std::string::npos);
    EXPECT_NE(p.html.find("<dd>1.5</dd>"), std::string::npos);
}

TEST(FrontmatterPanelTest, QuotedStringWithColon) {
    const auto p = Frontmatter::renderPanel("title: \"A: B\"\n");
    EXPECT_NE(p.html.find("<dd>A: B</dd>"), std::string::npos);
}

TEST(FrontmatterPanelTest, FoldedBlockScalar) {
    const auto p = Frontmatter::renderPanel("description: >\n  line one\n  line two\n");
    EXPECT_NE(p.html.find("line one line two"), std::string::npos);
}

TEST(FrontmatterPanelTest, LiteralBlockScalar) {
    const auto p = Frontmatter::renderPanel("description: |\n  a\n  b\n");
    EXPECT_NE(p.html.find("a\nb"), std::string::npos);
}

TEST(FrontmatterPanelTest, EscapesHtmlMetacharacters) {
    const auto p = Frontmatter::renderPanel("title: '<script>alert(\"x\")</script> & more'\n");
    EXPECT_EQ(p.html.find("<script>"), std::string::npos);
    EXPECT_NE(p.html.find("&lt;script&gt;"), std::string::npos);
    EXPECT_NE(p.html.find("&amp;"), std::string::npos);
    EXPECT_NE(p.html.find("&quot;"), std::string::npos);
}

TEST(FrontmatterPanelTest, EscapesKeysToo) {
    const auto p = Frontmatter::renderPanel("'<b>': v\n");
    EXPECT_EQ(p.html.find("<b>"), std::string::npos);
    EXPECT_NE(p.html.find("&lt;b&gt;"), std::string::npos);
}

TEST(FrontmatterPanelTest, KeysWithSpacesAndNumbers) {
    const auto p = Frontmatter::renderPanel("my key: v\n1: one\n");
    EXPECT_NE(p.html.find("<dt>my key</dt>"), std::string::npos);
    EXPECT_NE(p.html.find("<dt>1</dt><dd>one</dd>"), std::string::npos);
}

TEST(FrontmatterPanelTest, ExplicitStringTag) {
    const auto p = Frontmatter::renderPanel("value: !!str 123\n");
    EXPECT_NE(p.html.find("<dd>123</dd>"), std::string::npos);
}

TEST(FrontmatterPanelTest, LeadingComment) {
    const auto p = Frontmatter::renderPanel("# a comment\ntitle: x\n");
    EXPECT_NE(p.html.find("<dt>title</dt><dd>x</dd>"), std::string::npos);
}

TEST(FrontmatterPanelTest, AliasResolves) {
    const auto p = Frontmatter::renderPanel("base: &b value\ncopy: *b\n");
    EXPECT_NE(p.html.find("<dt>copy</dt><dd>value</dd>"), std::string::npos);
}

TEST(FrontmatterPanelTest, RecursiveAliasIsBounded) {
    const auto p = Frontmatter::renderPanel("a: &x\n  self: *x\n");
    EXPECT_LT(p.html.size(), 512u * 1024u);
}

TEST(FrontmatterPanelTest, MalformedYamlYieldsEmptyPanel) {
    const auto p = Frontmatter::renderPanel("title: [unclosed\n");
    EXPECT_TRUE(p.html.empty());
    EXPECT_FALSE(p.isOkf);
}

TEST(FrontmatterPanelTest, ScalarRootYieldsEmptyPanel) {
    EXPECT_TRUE(Frontmatter::renderPanel("just a string\n").html.empty());
}

TEST(FrontmatterPanelTest, SequenceRootYieldsEmptyPanel) {
    EXPECT_TRUE(Frontmatter::renderPanel("- a\n- b\n").html.empty());
}

TEST(FrontmatterPanelTest, EmptyYamlYieldsEmptyPanel) {
    EXPECT_TRUE(Frontmatter::renderPanel("").html.empty());
    EXPECT_TRUE(Frontmatter::renderPanel("   \n").html.empty());
}

TEST(FrontmatterPanelTest, OversizedYamlYieldsEmptyPanel) {
    const std::string yaml = "k: " + std::string(300 * 1024, 'x') + "\n";
    EXPECT_TRUE(Frontmatter::renderPanel(yaml).html.empty());
}

TEST(FrontmatterPanelTest, DuplicateKeysDoNotCrash) {
    const auto p = Frontmatter::renderPanel("a: 1\na: 2\n");
    if (!p.html.empty()) {
        EXPECT_NE(p.html.find("<dt>a</dt>"), std::string::npos);
    }
}

TEST(FrontmatterPanelTest, FullOkfDocument) {
    const auto p = Frontmatter::renderPanel(
        "type: table\ntitle: Customers\ntags: [sales, crm]\n"
        "generated:\n  by: agent/1.2.0\n  at: 2026-05-01T10:00:00Z\n"
        "verified:\n  - by: human:sasan\n    at: 2026-05-02T09:30:00Z\n"
        "sources:\n  - id: warehouse\n    resource: /db/warehouse.md\n    usage_count: 3\n");
    EXPECT_TRUE(p.isOkf);
    EXPECT_NE(p.html.find("<dt>title</dt><dd>Customers</dd>"), std::string::npos);
    EXPECT_NE(p.html.find("<span class=\"hyprmark-tag\">sales</span>"), std::string::npos);
    EXPECT_NE(p.html.find("<dt>id</dt><dd>warehouse</dd>"), std::string::npos);
    EXPECT_NE(p.html.find("<dt>usage_count</dt><dd>3</dd>"), std::string::npos);
    EXPECT_NE(p.html.find("<dt>by</dt><dd>human:sasan</dd>"), std::string::npos);
}

TEST(FrontmatterPanelTest, AttestedComputationFields) {
    const auto p = Frontmatter::renderPanel(
        "type: computation\nruntime: python3.12\n"
        "parameters:\n  - name: month\n    type: string\n    required: true\n"
        "executor:\n  resource: /executors/runner.md\n  receipt: /run-42.json\n"
        "attester:\n  resource: /attesters/auditor.md\n");
    EXPECT_TRUE(p.isOkf);
    EXPECT_NE(p.html.find("<dt>runtime</dt><dd>python3.12</dd>"), std::string::npos);
    EXPECT_NE(p.html.find("<dt>name</dt><dd>month</dd>"), std::string::npos);
    EXPECT_NE(p.html.find("<dt>required</dt><dd>true</dd>"), std::string::npos);
    EXPECT_NE(p.html.find("<dt>receipt</dt><dd>/run-42.json</dd>"), std::string::npos);
}

#endif // HYPRMARK_PARSE_FRONTMATTER

// ---------------------------------------------------------------------------
// Integration through CMarkdownRenderer.
// ---------------------------------------------------------------------------

class FrontmatterRenderTest : public ::testing::Test {
  protected:
    void SetUp() override {
        renderer.setAssetBase(std::filesystem::path(HYPRMARK_FIXTURES_DIR).parent_path().parent_path() / "assets");
    }
    CMarkdownRenderer renderer;
};

TEST_F(FrontmatterRenderTest, BlockStrippedFromBody) {
    const auto r = renderer.renderString("---\ntitle: T\n---\n# Heading\n");
    EXPECT_NE(r.html.find("<h1"), std::string::npos);
    EXPECT_NE(r.html.find("Heading"), std::string::npos);
    EXPECT_EQ(r.html.find("<hr"), std::string::npos);
    ASSERT_EQ(r.headings.size(), 1u);
    EXPECT_EQ(r.headings.front().text, "Heading");
}

TEST_F(FrontmatterRenderTest, YamlDoesNotBecomeAHeading) {
    const auto r = renderer.renderString("---\ntitle: not a heading\n---\n");
    EXPECT_EQ(r.html.find("<h2"), std::string::npos);
    EXPECT_EQ(r.html.find("<hr"), std::string::npos);
    EXPECT_TRUE(r.headings.empty());
}

TEST_F(FrontmatterRenderTest, NoFrontmatterNoPanel) {
    const auto r = renderer.renderString("# Heading\n");
    EXPECT_TRUE(r.metaHtml.empty());
    EXPECT_FALSE(r.isOkf);
    EXPECT_EQ(r.html.find("<details"), std::string::npos);
}

TEST_F(FrontmatterRenderTest, PanelPrependedInsideContent) {
    const auto r = renderer.renderString("---\ntitle: T\n---\n# Heading\n");
#ifdef HYPRMARK_PARSE_FRONTMATTER
    EXPECT_NE(r.metaHtml.find("hyprmark-meta"), std::string::npos);
    EXPECT_NE(r.fullPage.find("hyprmark-meta"), std::string::npos);
    EXPECT_EQ(r.html.rfind(r.metaHtml, 0), 0u); // panel is at the front
#else
    EXPECT_TRUE(r.metaHtml.empty());
#endif
}

TEST_F(FrontmatterRenderTest, OkfFlagPropagates) {
    const auto r = renderer.renderString("---\ntype: table\n---\n# H\n");
#ifdef HYPRMARK_PARSE_FRONTMATTER
    EXPECT_TRUE(r.isOkf);
#else
    EXPECT_FALSE(r.isOkf);
#endif
}

TEST_F(FrontmatterRenderTest, MalformedFrontmatterStillStripped) {
    const auto r = renderer.renderString("---\ntitle: [\n---\n# Heading\n");
    EXPECT_NE(r.html.find("<h1"), std::string::npos);
    EXPECT_EQ(r.html.find("<hr"), std::string::npos);
    ASSERT_EQ(r.headings.size(), 1u);
}

TEST_F(FrontmatterRenderTest, RenderFileWithFrontmatterFixture) {
    const auto r = renderer.renderFile(fixture("frontmatter-basic.md"));
    EXPECT_NE(r.html.find("Body Heading"), std::string::npos);
    EXPECT_EQ(r.html.find("title: Frontmatter Basic"), std::string::npos);
#ifdef HYPRMARK_PARSE_FRONTMATTER
    EXPECT_NE(r.metaHtml.find("<dd>Frontmatter Basic</dd>"), std::string::npos);
    EXPECT_NE(r.metaHtml.find("<span class=\"hyprmark-tag\">markdown</span>"), std::string::npos);
#endif
}

TEST_F(FrontmatterRenderTest, RenderFullOkfFixture) {
    const auto r = renderer.renderFile(fixture("okf-full.md"));
    EXPECT_NE(r.html.find("Customers"), std::string::npos);
#ifdef HYPRMARK_PARSE_FRONTMATTER
    EXPECT_TRUE(r.isOkf);
    EXPECT_NE(r.metaHtml.find("hyprmark-meta-badge"), std::string::npos);
    EXPECT_NE(r.metaHtml.find("<dt>resource</dt><dd>/tables/customers.md</dd>"), std::string::npos);
    EXPECT_NE(r.metaHtml.find("<dt>stale_after</dt><dd>2026-12-31T00:00:00Z</dd>"), std::string::npos);
    EXPECT_NE(r.metaHtml.find("<dt>id</dt><dd>warehouse</dd>"), std::string::npos);
#endif
}

TEST_F(FrontmatterRenderTest, RenderAttestedComputationFixture) {
    const auto r = renderer.renderFile(fixture("okf-attested.md"));
#ifdef HYPRMARK_PARSE_FRONTMATTER
    EXPECT_TRUE(r.isOkf);
    EXPECT_NE(r.metaHtml.find("<dt>runtime</dt><dd>python3.12</dd>"), std::string::npos);
    EXPECT_NE(r.metaHtml.find("<dt>computation</dt><dd>/computations/revenue.py</dd>"), std::string::npos);
    EXPECT_NE(r.metaHtml.find("<dt>attester</dt>"), std::string::npos);
#endif
}
