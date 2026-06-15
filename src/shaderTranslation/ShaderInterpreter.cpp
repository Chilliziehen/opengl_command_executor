/**
 * @file ShaderInterpreter.cpp
 * @brief Regex-based GLSL ES 3.0 → GLSL 3.30 translation
 */
#include "ShaderInterpreter.h"
#include <regex>

// ---- rule 1: #version 300 es → #version 330 core ----
void ShaderInterpreter::replaceVersion(std::string& source) {
    static const std::regex versionRegex(R"(#version\s+300\s+es)");
    // GLSL 410 has separate_shader_objects built-in (no extension needed for layout(location))
    source = std::regex_replace(source, versionRegex, "#version 410 core");
}

// ---- rule 2: remove precision * statements ----
void ShaderInterpreter::removePrecisionStatements(std::string& source) {
    // Matches: precision <qualifier> <type>;
    // e.g. precision highp float; precision mediump int; precision lowp sampler2D;
    static const std::regex precisionStmtRegex(
        R"(precision\s+(highp|mediump|lowp)\s+\w+\s*;)",
        std::regex::ECMAScript
    );
    source = std::regex_replace(source, precisionStmtRegex, "");
}

// ---- rule 3: remove precision qualifiers (highp/mediump/lowp) from declarations ----
void ShaderInterpreter::removePrecisionQualifiers(std::string& source) {
    // Remove standalone precision qualifiers like: in highp vec4 ...; uniform mediump sampler2D ...;
    // Also: layout(...) out mediump vec4
    // Pattern: \b(highp|mediump|lowp)\s+  — the qualifier followed by whitespace
    static const std::regex qualifierRegex(R"(\b(highp|mediump|lowp)\s+)");
    source = std::regex_replace(source, qualifierRegex, "");
}

// ---- rule 4: remove #extension directives ----
void ShaderInterpreter::removeExtensionDirectives(std::string& source) {
    // Match entire #extension ... : enable/disable/require lines
    static const std::regex extensionRegex(
        R"(#extension\s+\w+\s*:\s*(enable|disable|require)\s*\n?)",
        std::regex::ECMAScript
    );
    source = std::regex_replace(source, extensionRegex, "");
}

// ---- rule 5: add layout(std140) to UBO declarations ----
void ShaderInterpreter::addUboLayoutQualifier(std::string& source) {
    // UBO declarations look like:  uniform UnityPerDraw {
    // They appear at line start (after optional whitespace).
    // We add layout(std140) before them, unless they already have a layout qualifier.
    // Two-pass approach:
    //   Pass 1: find "layout(...) uniform" and mark them as skip zones
    //   Pass 2: for "uniform Identifier {" without layout, add it
    //
    // Simpler: match "uniform Identifier {" and verify it's not preceded by "layout"
    // by checking if the character before "uniform" is not 't' from "layout".
    // Pattern: uniform <Name> { at start of line (after optional ws)
    static const std::regex uboRegex(
        R"(^([ \t]*)uniform\s+(\w+)\s*\{)",
        std::regex::ECMAScript | std::regex::multiline
    );
    // Check each match — only replace if "layout" does not appear on the same line
    std::string result;
    result.reserve(source.size() * 1.1);
    size_t lastPos = 0;
    auto begin = std::sregex_iterator(source.begin(), source.end(), uboRegex);
    auto end   = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        const auto& match = *it;
        // Check if "layout" appears before "uniform" on this line
        // Search backwards from match position to start of line
        size_t matchPos = static_cast<size_t>(match.position());
        size_t lineStart = source.rfind('\n', matchPos);
        if (lineStart == std::string::npos) lineStart = 0; else ++lineStart;
        std::string linePrefix = source.substr(lineStart, matchPos - lineStart);
        bool hasLayout = (linePrefix.find("layout") != std::string::npos);

        result += source.substr(lastPos, matchPos - lastPos);
        if (!hasLayout) {
            result += match[1].str() + "layout(std140) uniform " + match[2].str() + " {";
        } else {
            result += match[0].str();
        }
        lastPos = matchPos + match.length();
    }
    result += source.substr(lastPos);
    source = std::move(result);
}

// ---- rule 6: add layout(location=0) to bare fragment shader outputs ----
void ShaderInterpreter::addFragmentOutputLayout(std::string& source) {
    // Match: "out vec4 SomeName;" or "out vec3 SomeName;" etc.
    // that is NOT preceded by "layout(location"
    // Fragment shader outputs in GLSL 330 core need explicit layout(location=N)
    // We handle the simple case: standalone "out" at global scope without layout.
    // Pattern: line start, optional ws, "out", ws, type, ws, name, ws, ";"
    static const std::regex outputRegex(
        R"(^([ \t]*)out\s+(\w+)\s+(\w+)\s*;)",
        std::regex::ECMAScript | std::regex::multiline
    );
    std::string result;
    result.reserve(source.size() * 1.1);
    size_t lastPos = 0;
    auto begin = std::sregex_iterator(source.begin(), source.end(), outputRegex);
    auto end   = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        const auto& match = *it;
        size_t matchPos = static_cast<size_t>(match.position());
        size_t lineStart = source.rfind('\n', matchPos);
        if (lineStart == std::string::npos) lineStart = 0; else ++lineStart;
        std::string linePrefix = source.substr(lineStart, matchPos - lineStart);
        bool hasLayout = (linePrefix.find("layout") != std::string::npos);

        result += source.substr(lastPos, matchPos - lastPos);
        if (!hasLayout) {
            result += match[1].str() + "layout(location = 0) out "
                   + match[2].str() + " " + match[3].str() + ";";
        } else {
            result += match[0].str();
        }
        lastPos = matchPos + match.length();
    }
    result += source.substr(lastPos);
    source = std::move(result);
}

// ---- public API ----

std::string ShaderInterpreter::translate(const std::string& glslEs300Source,
                                          std::string&       outError) {
    outError.clear();
    std::string result = glslEs300Source;

    if (result.empty()) {
        outError = "empty shader source";
        return {};
    }

    replaceVersion(result);
    removePrecisionStatements(result);
    removePrecisionQualifiers(result);
    removeExtensionDirectives(result);
    addUboLayoutQualifier(result);
    // Note: addFragmentOutputLayout intentionally NOT called.
    // In GLSL 410 core, bare "out vec4 color;" defaults to location 0.
    // Adding explicit layout(location=0) to vertex shader outputs would
    // assign all of them the same location, breaking multi-output shaders.

    return result;
}

bool ShaderInterpreter::translateInPlace(std::string& source,
                                          std::string& outError) {
    std::string translated = translate(source, outError);
    if (translated.empty() && !outError.empty())
        return false;
    source = std::move(translated);
    return true;
}
