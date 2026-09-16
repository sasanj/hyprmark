#pragma once

#include <string>

namespace Frontmatter {

    struct SParsed {
        bool        found = false; // a frontmatter block was present and well-formed
        std::string body;          // markdown with the block removed
        std::string yaml;          // raw YAML text of the block
    };

    // Splits a leading "---" YAML block off the document. Always compiled in:
    // removing the block keeps it out of the rendered body and the TOC. When no
    // block is present, `found` is false and `body` equals the input.
    SParsed split(const std::string& markdown);

    struct SPanel {
        std::string html;          // collapsed <details> panel; empty when nothing to show
        bool        isOkf = false; // a top-level `type` was present
    };

    // Renders the metadata panel from raw YAML. Returns an empty panel on parse
    // failure, on an oversized block, or when the document is not a mapping.
    SPanel renderPanel(const std::string& yaml);

} // namespace Frontmatter
