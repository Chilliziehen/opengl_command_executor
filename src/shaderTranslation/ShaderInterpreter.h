/**
 * @file ShaderInterpreter.h
 * @brief Translates WebGL GLSL ES 3.0 (300 es) → desktop GLSL 3.30 (330 core)
 */
#ifndef SHADER_INTERPRETER_H
#define SHADER_INTERPRETER_H

#include <string>

class ShaderInterpreter {
public:
    /// Translate a single GLSL ES 3.0 source string to GLSL 3.30.
    /// Returns the translated source on success, or empty string on failure
    /// (with details written to outError).
    static std::string translate(const std::string& glslEs300Source,
                                 std::string&       outError);

    /// Convenience: translate in place, returns true on success.
    static bool translateInPlace(std::string& source,
                                 std::string& outError);

private:
    ShaderInterpreter() = delete;

    static void removePrecisionStatements(std::string& source);
    static void removePrecisionQualifiers(std::string& source);
    static void removeExtensionDirectives(std::string& source);
    static void addUboLayoutQualifier(std::string& source);
    static void addFragmentOutputLayout(std::string& source);
    static void replaceVersion(std::string& source);
};

#endif
