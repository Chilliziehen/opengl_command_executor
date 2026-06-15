#include "ResourceManager.h"
#include "mappers/UniversalDMapper.h"
#include "framecapture/FrameCapture.h"
#include "drawResources/Buffer.h"
#include "drawResources/Texture.h"
#include "drawResources/Shader.h"
#include "drawResources/Program.h"
#include "drawResources/State.h"
#include "shaderTranslation/ShaderInterpreter.h"
#include <glad/gl.h>
#include <fstream>
#include <stdexcept>
#include <vector>

static std::string joinPathLocal(const std::string& directory, const std::string& relative) {
    if (relative.empty()) return "";
    if (directory.empty()) return relative;
    if (directory.back() == '/' || directory.back() == '\\')
        return directory + relative;
    return directory + "/" + relative;
}

static std::vector<char> readBinaryFileLocal(const std::string& filePath, std::string& outError) {
    std::ifstream stream(filePath, std::ios::binary | std::ios::ate);
    if (!stream.is_open()) {
        outError = "cannot open binary file: " + filePath;
        return {};
    }
    std::streamsize size = stream.tellg();
    stream.seekg(0, std::ios::beg);
    std::vector<char> data(static_cast<size_t>(size));
    if (!stream.read(data.data(), size)) {
        outError = "failed to read binary file: " + filePath;
        return {};
    }
    return data;
}

bool ResourceManager::uploadBufferData(const CaptureBuffer& metadata,
                                        const std::string& captureDirectoryPath,
                                        std::string& outError) {
    if (!hasMappedHandle(ResourceKind::Buffer, metadata.m_bufferId)) {
        outError = "Buffer " + std::to_string(metadata.m_bufferId) + " not allocated";
        return false;
    }
    GLuint handle = getMappedHandle(ResourceKind::Buffer, metadata.m_bufferId);
    glBindBuffer(metadata.m_target, handle);

    if (metadata.m_binaryDataPath.empty()) {
        glBufferData(metadata.m_target, static_cast<GLsizeiptr>(metadata.m_byteLength),
                     nullptr, metadata.m_usageHint);
        return true;
    }

    std::string fullPath = joinPathLocal(captureDirectoryPath, metadata.m_binaryDataPath);
    auto data = readBinaryFileLocal(fullPath, outError);
    if (data.empty() && !outError.empty()) return false;

    glBufferData(metadata.m_target, static_cast<GLsizeiptr>(data.size()),
                 data.data(), metadata.m_usageHint);
    return true;
}

bool ResourceManager::uploadTextureData(const TextureWrapper& metadata,
                                         const std::string& captureDirectoryPath,
                                         std::string& outError) {
    if (!hasMappedHandle(ResourceKind::Texture, metadata.m_textureId)) {
        outError = "Texture " + std::to_string(metadata.m_textureId) + " not allocated";
        return false;
    }
    GLuint handle = getMappedHandle(ResourceKind::Texture, metadata.m_textureId);
    glBindTexture(metadata.m_target, handle);

    const void* pixelData = nullptr;
    std::vector<char> data;
    if (!metadata.m_textureBinaryPath.empty()) {
        std::string fullPath = joinPathLocal(captureDirectoryPath, metadata.m_textureBinaryPath);
        data = readBinaryFileLocal(fullPath, outError);
        if (data.empty() && !outError.empty()) return false;
        pixelData = data.data();
    }

    // capturedWidth/Height may differ from width/height (downsampled 1/4)
    GLsizei uploadWidth  = pixelData ? static_cast<GLsizei>(metadata.m_capturedWidth)  : static_cast<GLsizei>(metadata.m_width);
    GLsizei uploadHeight = pixelData ? static_cast<GLsizei>(metadata.m_capturedHeight) : static_cast<GLsizei>(metadata.m_height);
    glTexImage2D(metadata.m_target, 0,
                 static_cast<GLint>(metadata.m_internalFormat),
                 uploadWidth, uploadHeight, 0,
                 metadata.m_format, metadata.m_pixelType, pixelData);

    for (const auto& [key, val] : metadata.m_parameters) {
        GLenum parameterName = static_cast<GLenum>(std::stoul(key));
        glTexParameteri(metadata.m_target, parameterName, val);
    }
    return true;
}

bool ResourceManager::compileShader(const CaptureShader& metadata,
                                     std::string& outError) {
    GLuint handle = getMappedHandle(ResourceKind::Shader, metadata.m_shaderId);

    // Translate GLSL ES 3.0 → desktop GLSL 3.30
    std::string glslSource = metadata.m_source;
    std::string translateError;
    if (!ShaderInterpreter::translateInPlace(glslSource, translateError)) {
        outError = "Shader #" + std::to_string(metadata.m_shaderId)
                 + " translation failed: " + translateError;
        return false;
    }

    const char* sourcePtr = glslSource.c_str();
    glShaderSource(handle, 1, &sourcePtr, nullptr);
    glCompileShader(handle);

    GLint compiled = 0;
    glGetShaderiv(handle, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        GLint length = 0;
        glGetShaderiv(handle, GL_INFO_LOG_LENGTH, &length);
        if (length > 0) {
            std::vector<char> logBuffer(static_cast<size_t>(length));
            glGetShaderInfoLog(handle, length, nullptr, logBuffer.data());
            outError = "Shader #" + std::to_string(metadata.m_shaderId)
                     + " compile failed: " + std::string(logBuffer.data());
        } else {
            outError = "Shader #" + std::to_string(metadata.m_shaderId)
                     + " compile failed (no log)";
        }
        return false;
    }
    return true;
}

bool ResourceManager::linkProgram(const CaptureProgram& metadata,
                                   std::string& outError) {
    GLuint programHandle = getMappedHandle(ResourceKind::Program, metadata.m_programId);

    for (uint32_t captureShaderId : metadata.m_attachedShaderIds) {
        GLuint shaderHandle = getMappedHandle(ResourceKind::Shader, captureShaderId);
        glAttachShader(programHandle, shaderHandle);
    }

    glLinkProgram(programHandle);
    GLint linked = 0;
    glGetProgramiv(programHandle, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLint length = 0;
        glGetProgramiv(programHandle, GL_INFO_LOG_LENGTH, &length);
        std::vector<char> log(static_cast<size_t>(length));
        glGetProgramInfoLog(programHandle, length, nullptr, log.data());
        outError = "Program #" + std::to_string(metadata.m_programId)
                 + " link failed: " + std::string(log.data());
        return false;
    }
    return true;
}

bool ResourceManager::restoreState(const CaptureState& state,
                                    std::string& /*outError*/) {
    // ---- program ----
    if (hasMappedHandle(ResourceKind::Program, state.m_currentProgramId))
        glUseProgram(getMappedHandle(ResourceKind::Program, state.m_currentProgramId));

    // ---- array buffer ----
    if (state.m_currentArrayBufferId != 0 && hasMappedHandle(ResourceKind::Buffer, state.m_currentArrayBufferId))
        glBindBuffer(GL_ARRAY_BUFFER, getMappedHandle(ResourceKind::Buffer, state.m_currentArrayBufferId));

    // ---- element array buffer ----
    if (state.m_currentElementArrayBufferId != 0 && hasMappedHandle(ResourceKind::Buffer, state.m_currentElementArrayBufferId))
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, getMappedHandle(ResourceKind::Buffer, state.m_currentElementArrayBufferId));

    // ---- active texture unit + texture bindings + sampler bindings ----
    glActiveTexture(GL_TEXTURE0 + state.m_currentActiveTextureUnit);

    for (const auto& [unitString, textureCaptureId] : state.m_currentTextureBindings) {
        int unit = std::stoi(unitString);
        glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(unit));
        if (textureCaptureId != 0 && hasMappedHandle(ResourceKind::Texture, textureCaptureId))
            glBindTexture(GL_TEXTURE_2D, getMappedHandle(ResourceKind::Texture, textureCaptureId));
        else if (textureCaptureId == 0)
            glBindTexture(GL_TEXTURE_2D, 0);
    }

    for (const auto& [programIdString, samplerMap] : state.m_currentSamplerBindings) {
        uint32_t programId = static_cast<uint32_t>(std::stoul(programIdString));
        if (!hasMappedHandle(ResourceKind::Program, programId)) continue;
        glUseProgram(getMappedHandle(ResourceKind::Program, programId));
        for (const auto& [uniformName, textureUnit] : samplerMap) {
            GLint location = glGetUniformLocation(getMappedHandle(ResourceKind::Program, programId), uniformName.c_str());
            if (location >= 0)
                glUniform1i(location, static_cast<GLint>(textureUnit));
        }
    }
    // Restore current program after sampler setup
    if (hasMappedHandle(ResourceKind::Program, state.m_currentProgramId))
        glUseProgram(getMappedHandle(ResourceKind::Program, state.m_currentProgramId));

    // NOTE: VAO/FBO bindings + vertex attributes are handled by
    //       restoreVertexArrays() / restoreFramebuffers() respectively

    // ---- viewport ----
    if (state.m_viewport.size() >= 4)
        glViewport(state.m_viewport[0], state.m_viewport[1],
                   state.m_viewport[2], state.m_viewport[3]);

    // ---- clear values ----
    if (state.m_clearColor.size() >= 4)
        glClearColor(state.m_clearColor[0], state.m_clearColor[1],
                     state.m_clearColor[2], state.m_clearColor[3]);
#ifdef GL_ES_VERSION_3_0
    glClearDepthf(state.m_clearDepth);
#else
    glClearDepth(static_cast<double>(state.m_clearDepth));
#endif
    glClearStencil(state.m_clearStencil);

    // ---- depth ----
    glDepthMask(state.m_depthMask ? GL_TRUE : GL_FALSE);
    glDepthFunc(state.m_depthFunction);

    // ---- culling ----
    glCullFace(state.m_cullFaceMode);
    glFrontFace(state.m_frontFaceOrientation);

    // ---- color mask ----
    if (state.m_colorMask.size() >= 4)
        glColorMask(state.m_colorMask[0] ? GL_TRUE : GL_FALSE,
                    state.m_colorMask[1] ? GL_TRUE : GL_FALSE,
                    state.m_colorMask[2] ? GL_TRUE : GL_FALSE,
                    state.m_colorMask[3] ? GL_TRUE : GL_FALSE);

    // ---- scissor ----
    if (state.m_scissorBox.size() >= 4)
        glScissor(state.m_scissorBox[0], state.m_scissorBox[1],
                  state.m_scissorBox[2], state.m_scissorBox[3]);

    // ---- blend ----
    glBlendFuncSeparate(state.m_blendFunction.m_sourceRgb,
                        state.m_blendFunction.m_destinationRgb,
                        state.m_blendFunction.m_sourceAlpha,
                        state.m_blendFunction.m_destinationAlpha);
    glBlendEquationSeparate(state.m_blendEquation.m_rgbMode,
                            state.m_blendEquation.m_alphaMode);

    // ---- capabilities ----
    for (const auto& [capability, enabled] : state.m_capabilities) {
        GLenum capEnum = static_cast<GLenum>(std::stoul(capability));
        if (enabled) glEnable(capEnum);
        else         glDisable(capEnum);
    }

    // ---- pixel store ----
    for (const auto& [parameterKey, parameterValue] : state.m_pixelStoreParameters) {
        GLenum paramName = static_cast<GLenum>(std::stoul(parameterKey));
        glPixelStorei(paramName, parameterValue);
    }

    return true;
}

bool ResourceManager::uploadAllResources(const FrameCapture& capture,
                                          const std::string& captureDirectoryPath,
                                          std::string& outError) {
    for (const auto& b : capture.m_buffers)
        if (!uploadBufferData(b, captureDirectoryPath, outError)) return false;
    for (const auto& t : capture.m_textures)
        if (!uploadTextureData(t, captureDirectoryPath, outError)) return false;
    for (const auto& s : capture.m_shaders)
        if (!compileShader(s, outError)) return false;
    for (const auto& p : capture.m_programs)
        if (!linkProgram(p, outError)) return false;
    if (!restoreState(capture.m_state, outError)) return false;
    if (!restoreFramebuffers(capture, outError)) return false;
    if (!restoreVertexArrays(capture, outError)) return false;
    return true;
}

// ---- FBO attachment restoration ----

bool ResourceManager::restoreFramebuffers(const FrameCapture& capture,
                                           std::string& outError) {
    for (const auto& framebuffer : capture.m_framebuffers) {
        if (!hasMappedHandle(ResourceKind::Framebuffer, framebuffer.m_framebufferId)) {
            outError = "FBO #" + std::to_string(framebuffer.m_framebufferId) + " not allocated";
            return false;
        }
        GLuint fboHandle = getMappedHandle(ResourceKind::Framebuffer, framebuffer.m_framebufferId);
        glBindFramebuffer(GL_FRAMEBUFFER, fboHandle);

        for (const auto& attachment : framebuffer.m_attachments) {
            // textureId == 0 means a renderbuffer, which we don't handle yet
            if (attachment.m_textureId == 0) continue;

            if (!hasMappedHandle(ResourceKind::Texture, attachment.m_textureId)) {
                outError = "Texture #" + std::to_string(attachment.m_textureId)
                         + " for FBO #" + std::to_string(framebuffer.m_framebufferId)
                         + " attachment not allocated";
                return false;
            }
            GLuint textureHandle = getMappedHandle(ResourceKind::Texture, attachment.m_textureId);

            glFramebufferTexture2D(
                GL_FRAMEBUFFER,
                attachment.m_attachmentPoint,
                attachment.m_textureTarget,
                textureHandle,
                static_cast<GLint>(attachment.m_mipmapLevel)
            );
        }
    }

    // Restore the frame-start FBO binding (0 = default framebuffer)
    if (capture.m_state.m_currentFramebufferId == 0) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    } else if (hasMappedHandle(ResourceKind::Framebuffer, capture.m_state.m_currentFramebufferId)) {
        glBindFramebuffer(GL_FRAMEBUFFER,
            getMappedHandle(ResourceKind::Framebuffer, capture.m_state.m_currentFramebufferId));
    }
    return true;
}

// ---- VAO binding restoration ----

bool ResourceManager::restoreVertexArrays(const FrameCapture& capture,
                                           std::string& outError) {
    for (const auto& vertexArray : capture.m_vertexArrays) {
        if (vertexArray.m_deleted) continue;

        if (!hasMappedHandle(ResourceKind::VertexArray, vertexArray.m_vertexArrayId)) {
            outError = "VAO #" + std::to_string(vertexArray.m_vertexArrayId) + " not allocated";
            return false;
        }
        GLuint vaoHandle = getMappedHandle(ResourceKind::VertexArray, vertexArray.m_vertexArrayId);
        glBindVertexArray(vaoHandle);

        // Bind the element array buffer (EBO) if present
        if (vertexArray.m_elementArrayBufferId != 0) {
            if (!hasMappedHandle(ResourceKind::Buffer, vertexArray.m_elementArrayBufferId)) {
                outError = "EBO #" + std::to_string(vertexArray.m_elementArrayBufferId)
                         + " for VAO #" + std::to_string(vertexArray.m_vertexArrayId) + " not allocated";
                return false;
            }
            GLuint eboHandle = getMappedHandle(ResourceKind::Buffer, vertexArray.m_elementArrayBufferId);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, eboHandle);
        }

        // Set up each vertex attribute
        for (const auto& [indexString, attribute] : vertexArray.m_vertexAttributes) {
            uint32_t attributeIndex = static_cast<uint32_t>(std::stoul(indexString));

            // Bind the attribute's source VBO to GL_ARRAY_BUFFER
            if (attribute.m_bufferId != 0) {
                if (!hasMappedHandle(ResourceKind::Buffer, attribute.m_bufferId)) {
                    outError = "VBO #" + std::to_string(attribute.m_bufferId)
                             + " for VAO #" + std::to_string(vertexArray.m_vertexArrayId)
                             + " attrib " + indexString + " not allocated";
                    return false;
                }
                GLuint vboHandle = getMappedHandle(ResourceKind::Buffer, attribute.m_bufferId);
                glBindBuffer(GL_ARRAY_BUFFER, vboHandle);
            }

            // Configure the vertex attribute pointer
            glVertexAttribPointer(
                attributeIndex,
                static_cast<GLint>(attribute.m_componentCount),
                attribute.m_componentType,
                attribute.m_normalized ? GL_TRUE : GL_FALSE,
                static_cast<GLsizei>(attribute.m_stride),
                reinterpret_cast<const void*>(static_cast<uintptr_t>(attribute.m_offset))
            );

            // Enable or disable the array
            if (attribute.m_enabled) {
                glEnableVertexAttribArray(attributeIndex);
            } else {
                glDisableVertexAttribArray(attributeIndex);
            }
        }
    }

    // ---- state snapshot's global vertex attributes (on VAO #0 or default VAO) ----
    const auto& state = capture.m_state;
    if (!state.m_vertexAttributes.empty()) {
        bool hasDefaultVertexArray = (state.m_currentVertexArrayId == 0)
            || (state.m_currentVertexArrayId != 0 && hasMappedHandle(ResourceKind::VertexArray, state.m_currentVertexArrayId));
        if (hasDefaultVertexArray) {
            GLuint stateVaoHandle = (state.m_currentVertexArrayId == 0)
                ? 0
                : getMappedHandle(ResourceKind::VertexArray, state.m_currentVertexArrayId);
            glBindVertexArray(stateVaoHandle);

            for (const auto& [indexString, attribute] : state.m_vertexAttributes) {
                uint32_t attrIndex = static_cast<uint32_t>(std::stoul(indexString));
                if (attribute.m_bufferId != 0 && hasMappedHandle(ResourceKind::Buffer, attribute.m_bufferId))
                    glBindBuffer(GL_ARRAY_BUFFER, getMappedHandle(ResourceKind::Buffer, attribute.m_bufferId));
                glVertexAttribPointer(attrIndex,
                                      static_cast<GLint>(attribute.m_componentCount),
                                      attribute.m_componentType,
                                      attribute.m_normalized ? GL_TRUE : GL_FALSE,
                                      static_cast<GLsizei>(attribute.m_stride),
                                      reinterpret_cast<const void*>(static_cast<uintptr_t>(attribute.m_offset)));
                if (attribute.m_enabled) glEnableVertexAttribArray(attrIndex);
                else                     glDisableVertexAttribArray(attrIndex);
            }
        }
    }

    // Unbind everything to leave a clean state for command execution
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    return true;
}
