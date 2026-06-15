/**
 * @file testShaderTranslate.cpp
 * @brief Translates all 12 shaders and prints results (no GL needed)
 */
#include <iostream>
#include "shaderTranslation/ShaderInterpreter.h"
#include "framecapture/CaptureLoader.h"

int main(int argc, char* argv[]) {
    std::string capturePath = (argc > 1)
        ? argv[1]
        : std::string(PROJECT_ROOT_DIR) + "/example/20260611_161020_frame6";

    FrameCapture capture;
    std::string  error;
    if (!CaptureLoader::loadFromDirectory(capturePath, capture, error)) {
        std::cerr << "Load FAILED: " << error << std::endl;
        return 1;
    }

    std::cout << "Loaded " << capture.shaderCount() << " shaders.\n" << std::endl;

    for (const auto& shader : capture.m_shaders) {
        std::string translated = shader.m_source;
        std::string translateError;
        bool ok = ShaderInterpreter::translateInPlace(translated, translateError);
        const char* typeStr = (shader.m_shaderType == 35633) ? "VERTEX" : "FRAGMENT";

        if (!ok) {
            std::cerr << "Shader #" << shader.m_shaderId << " (" << typeStr
                      << ") TRANSLATE FAILED: " << translateError << std::endl;
            return 1;
        }

        // Quick sanity checks
        bool hasVersionEs = (translated.find("#version 300 es") != std::string::npos);
        bool hasPrecision  = (translated.find("precision ") != std::string::npos);
        bool hasHighp      = (translated.find("highp ")   != std::string::npos);
        bool hasExtension  = (translated.find("#extension") != std::string::npos);

        std::string issues;
        if (hasVersionEs)  issues += " version-es";
        if (hasPrecision)  issues += " precision-stmt";
        if (hasHighp)      issues += " highp";
        if (hasExtension)  issues += " extension";

        std::cout << "Shader #" << shader.m_shaderId << " (" << typeStr
                  << "): " << shader.m_source.size() << " -> "
                  << translated.size() << " bytes"
                  << (issues.empty() ? " OK" : " REMAINING:" + issues)
                  << std::endl;
    }

    std::cout << "\nAll shaders translated successfully." << std::endl;
    return 0;
}
