#include "Frontmatter.hpp"

#include <string_view>

namespace Frontmatter {

    namespace {
        struct SDelimiter {
            size_t start = 0; // offset of the "---" line
            size_t after = 0; // offset just past that line (including its newline)
        };

        bool isDelimiterLine(std::string_view line) {
            while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t'))
                line.remove_suffix(1);
            return line == "---";
        }

        // Finds the next line that is exactly "---", starting at `from`.
        bool findClosingDelimiter(const std::string& in, size_t from, SDelimiter& out) {
            size_t pos = from;
            while (pos <= in.size()) {
                const auto eol     = in.find('\n', pos);
                const auto lineEnd = (eol == std::string::npos) ? in.size() : eol;

                std::string_view line(in.data() + pos, lineEnd - pos);
                if (isDelimiterLine(line)) {
                    out.start = pos;
                    out.after = (eol == std::string::npos) ? in.size() : eol + 1;
                    return true;
                }

                if (eol == std::string::npos)
                    break;
                pos = eol + 1;
            }
            return false;
        }
    } // namespace

    SParsed split(const std::string& markdown) {
        SParsed result;
        result.body = markdown;

        size_t pos = 0;
        if (markdown.size() >= 3 && static_cast<unsigned char>(markdown[0]) == 0xEF &&
            static_cast<unsigned char>(markdown[1]) == 0xBB && static_cast<unsigned char>(markdown[2]) == 0xBF)
            pos = 3;

        // The opening delimiter must be the very first line.
        const auto eol      = markdown.find('\n', pos);
        const auto firstEnd = (eol == std::string::npos) ? markdown.size() : eol;
        if (!isDelimiterLine(std::string_view(markdown.data() + pos, firstEnd - pos)))
            return result;

        const size_t contentStart = (eol == std::string::npos) ? markdown.size() : eol + 1;

        SDelimiter delim;
        if (!findClosingDelimiter(markdown, contentStart, delim))
            return result; // no closing delimiter: render as normal markdown

        result.found = true;
        result.yaml  = markdown.substr(contentStart, delim.start - contentStart);
        result.body  = markdown.substr(delim.after); // drops the BOM and the block
        return result;
    }

} // namespace Frontmatter

#ifdef HYPRMARK_PARSE_FRONTMATTER

#include "../helpers/Log.hpp"

#include <yaml-cpp/yaml.h>

#include <cctype>
#include <filesystem>

namespace Frontmatter {

    namespace {
        constexpr size_t kMaxFrontmatterBytes = 256 * 1024;
        constexpr size_t kMaxPanelBytes       = 512 * 1024;
        constexpr int    kMaxDepth            = 16;

        std::string htmlEscape(const std::string& s) {
            std::string out;
            out.reserve(s.size());
            for (char c : s) {
                switch (c) {
                    case '&': out += "&amp;"; break;
                    case '<': out += "&lt;"; break;
                    case '>': out += "&gt;"; break;
                    case '"': out += "&quot;"; break;
                    default: out.push_back(c);
                }
            }
            return out;
        }

        std::string scalarText(const YAML::Node& n) {
            return n.IsScalar() ? n.Scalar() : "";
        }

        bool isSkillFileName(const std::string& sourcePath) {
            if (sourcePath.empty())
                return false;

            const std::string name = std::filesystem::path(sourcePath).filename().string();
            static constexpr std::string_view kSkillFile = "skill.md";
            if (name.size() != kSkillFile.size())
                return false;

            for (size_t i = 0; i < name.size(); ++i) {
                if (std::tolower(static_cast<unsigned char>(name[i])) != kSkillFile[i])
                    return false;
            }
            return true;
        }

        bool hasSkillShape(const YAML::Node& root) {
            return root["name"] && root["name"].IsScalar() && root["description"] && root["description"].IsScalar();
        }

        void renderNode(const YAML::Node& node, std::string& out, int depth);

        void renderMap(const YAML::Node& map, std::string& out, int depth) {
            out += "<dl class=\"hyprmark-meta-list\">";
            for (const auto& kv : map) {
                if (out.size() > kMaxPanelBytes)
                    break;

                const std::string key = scalarText(kv.first);
                out += "<dt>" + htmlEscape(key) + "</dt>";

                const auto& val = kv.second;
                if (key == "tags" && val.IsSequence()) {
                    out += "<dd class=\"hyprmark-tags\">";
                    for (const auto& t : val) {
                        if (t.IsScalar())
                            out += "<span class=\"hyprmark-tag\">" + htmlEscape(scalarText(t)) + "</span>";
                        else
                            renderNode(t, out, depth + 1);
                    }
                    out += "</dd>";
                } else if (val.IsScalar()) {
                    out += "<dd>" + htmlEscape(scalarText(val)) + "</dd>";
                } else {
                    out += "<dd>";
                    renderNode(val, out, depth + 1);
                    out += "</dd>";
                }
            }
            out += "</dl>";
        }

        void renderNode(const YAML::Node& node, std::string& out, int depth) {
            if (depth > kMaxDepth || out.size() > kMaxPanelBytes)
                return;

            if (node.IsMap()) {
                renderMap(node, out, depth);
            } else if (node.IsSequence()) {
                out += "<ul class=\"hyprmark-meta-seq\">";
                for (const auto& item : node) {
                    if (out.size() > kMaxPanelBytes)
                        break;
                    out += "<li>";
                    if (item.IsScalar())
                        out += htmlEscape(scalarText(item));
                    else
                        renderNode(item, out, depth + 1);
                    out += "</li>";
                }
                out += "</ul>";
            } else if (node.IsScalar()) {
                out += htmlEscape(scalarText(node));
            }
        }
    } // namespace

    SPanel renderPanel(const std::string& yaml, const std::string& sourcePath) {
        SPanel panel;
        if (yaml.size() > kMaxFrontmatterBytes)
            return panel;

        YAML::Node root;
        try {
            root = YAML::Load(yaml);
        } catch (const std::exception& e) {
            Debug::log(ERR, "frontmatter parse failed: {}; ignoring block", e.what());
            return panel;
        }

        if (!root.IsMap())
            return panel;

        panel.isOkf   = root["type"] && root["type"].IsScalar();
        panel.isSkill = isSkillFileName(sourcePath) || hasSkillShape(root);

        std::string out = "<details class=\"hyprmark-meta\">";
        out += "<summary>Metadata";
        if (panel.isOkf)
            out += " <span class=\"hyprmark-meta-badge\" title=\"Open Knowledge Format (OKF) v0.2\">OKF</span>";
        if (panel.isSkill)
            out += " <span class=\"hyprmark-meta-badge hyprmark-meta-badge-skill\" title=\"Agent Skill (SKILL.md)\">SKILL</span>";
        out += "</summary>";
        renderMap(root, out, 0);
        out += "</details>";

        panel.html = std::move(out);
        return panel;
    }

} // namespace Frontmatter

#else // !HYPRMARK_PARSE_FRONTMATTER

namespace Frontmatter {
    SPanel renderPanel(const std::string&, const std::string&) {
        return {};
    }
} // namespace Frontmatter

#endif
