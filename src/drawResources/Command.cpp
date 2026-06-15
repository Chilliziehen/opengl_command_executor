#include "Command.h"
#include "mappers/UniversalDMapper.h"
#include "resourceManagement/ResourceAllocator.h"
#include <fstream>
#include <glad/gl.h>
#include <vector>

std::string Command::s_captureDirectory;

// ---- state commands ----

ViewportCommand::ViewportCommand(uint32_t eventId, std::vector<int32_t> bounds)
    : Command(eventId, "viewport"), m_bounds(std::move(bounds)) {}
void ViewportCommand::execute() {
  if (m_bounds.size() >= 4)
    glViewport(m_bounds[0], m_bounds[1], m_bounds[2], m_bounds[3]);
}

EnableCommand::EnableCommand(uint32_t eventId, uint32_t capability, bool enable)
    : Command(eventId, enable ? "enable" : "disable"), m_capability(capability),
      m_enable(enable) {}
void EnableCommand::execute() {
  if (m_enable)
    glEnable(m_capability);
  else
    glDisable(m_capability);
}

ClearColorCommand::ClearColorCommand(uint32_t eventId, float r, float g,
                                     float b, float a)
    : Command(eventId, "clearColor"), m_red(r), m_green(g), m_blue(b),
      m_alpha(a) {}
void ClearColorCommand::execute() {
  glClearColor(m_red, m_green, m_blue, m_alpha);
}

ClearCommand::ClearCommand(uint32_t eventId, uint32_t mask,
                           uint32_t framebufferId)
    : Command(eventId, "clear"), m_mask(mask), m_framebufferId(framebufferId) {}
void ClearCommand::execute() {
  if (m_framebufferId != 0) {
    if (hasMappedHandle(ResourceKind::Framebuffer, m_framebufferId))
      glBindFramebuffer(
          GL_FRAMEBUFFER,
          getMappedHandle(ResourceKind::Framebuffer, m_framebufferId));
  } else {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }
  glClear(m_mask);
}

ScissorCommand::ScissorCommand(uint32_t eventId, std::vector<int32_t> box)
    : Command(eventId, "scissor"), m_box(std::move(box)) {}
void ScissorCommand::execute() {
  if (m_box.size() >= 4)
    glScissor(m_box[0], m_box[1], m_box[2], m_box[3]);
}

BlendFunctionCommand::BlendFunctionCommand(uint32_t eventId,
                                           uint32_t sourceFactor,
                                           uint32_t destinationFactor)
    : Command(eventId, "blendFunc"), m_sourceFactor(sourceFactor),
      m_destinationFactor(destinationFactor) {}
void BlendFunctionCommand::execute() {
  glBlendFunc(m_sourceFactor, m_destinationFactor);
}

BlendEquationCommand::BlendEquationCommand(uint32_t eventId, uint32_t mode)
    : Command(eventId, "blendEquation"), m_mode(mode) {}
void BlendEquationCommand::execute() { glBlendEquation(m_mode); }

CullFaceCommand::CullFaceCommand(uint32_t eventId, uint32_t mode)
    : Command(eventId, "cullFace"), m_mode(mode) {}
void CullFaceCommand::execute() { glCullFace(m_mode); }

FrontFaceCommand::FrontFaceCommand(uint32_t eventId, uint32_t orientation)
    : Command(eventId, "frontFace"), m_orientation(orientation) {}
void FrontFaceCommand::execute() { glFrontFace(m_orientation); }

DepthFunctionCommand::DepthFunctionCommand(uint32_t eventId, uint32_t function)
    : Command(eventId, "depthFunc"), m_function(function) {}
void DepthFunctionCommand::execute() { glDepthFunc(m_function); }

DepthMaskCommand::DepthMaskCommand(uint32_t eventId, bool enabled)
    : Command(eventId, "depthMask"), m_enabled(enabled) {}
void DepthMaskCommand::execute() {
  glDepthMask(m_enabled ? GL_TRUE : GL_FALSE);
}

ColorMaskCommand::ColorMaskCommand(uint32_t eventId, bool red, bool green,
                                   bool blue, bool alpha)
    : Command(eventId, "colorMask"), m_red(red), m_green(green), m_blue(blue),
      m_alpha(alpha) {}
void ColorMaskCommand::execute() {
  glColorMask(m_red, m_green, m_blue, m_alpha);
}

LineWidthCommand::LineWidthCommand(uint32_t eventId, float width)
    : Command(eventId, "lineWidth"), m_width(width) {}
void LineWidthCommand::execute() { glLineWidth(m_width); }

StencilFunctionCommand::StencilFunctionCommand(uint32_t eventId,
                                               uint32_t function,
                                               int32_t reference, uint32_t mask)
    : Command(eventId, "stencilFunc"), m_function(function),
      m_reference(reference), m_mask(mask) {}
void StencilFunctionCommand::execute() {
  glStencilFunc(m_function, m_reference, m_mask);
}

StencilOperationCommand::StencilOperationCommand(uint32_t eventId,
                                                 uint32_t stencilFail,
                                                 uint32_t depthFail,
                                                 uint32_t depthPass)
    : Command(eventId, "stencilOp"), m_stencilFail(stencilFail),
      m_depthFail(depthFail), m_depthPass(depthPass) {}
void StencilOperationCommand::execute() {
  glStencilOp(m_stencilFail, m_depthFail, m_depthPass);
}

StencilMaskCommand::StencilMaskCommand(uint32_t eventId, uint32_t mask)
    : Command(eventId, "stencilMask"), m_mask(mask) {}
void StencilMaskCommand::execute() { glStencilMask(m_mask); }

BlendColorCommand::BlendColorCommand(uint32_t eventId, float r, float g,
                                     float b, float a)
    : Command(eventId, "blendColor"), m_red(r), m_green(g), m_blue(b),
      m_alpha(a) {}
void BlendColorCommand::execute() {
  glBlendColor(m_red, m_green, m_blue, m_alpha);
}

// ---- resource-binding commands ----

UseProgramCommand::UseProgramCommand(uint32_t eventId, uint32_t programId)
    : Command(eventId, "useProgram"), m_programId(programId) {}
void UseProgramCommand::execute() {
  if (hasMappedHandle(ResourceKind::Program, m_programId))
    glUseProgram(getMappedHandle(ResourceKind::Program, m_programId));
}

BindFramebufferCommand::BindFramebufferCommand(uint32_t eventId,
                                               uint32_t target,
                                               uint32_t framebufferId)
    : Command(eventId, "bindFramebuffer"), m_target(target),
      m_framebufferId(framebufferId) {}
void BindFramebufferCommand::execute() {
  GLuint handle = 0;
  if (m_framebufferId != 0) {
    if (hasMappedHandle(ResourceKind::Framebuffer, m_framebufferId))
      handle = getMappedHandle(ResourceKind::Framebuffer, m_framebufferId);
  }
  glBindFramebuffer(m_target, handle);
}

BindBufferCommand::BindBufferCommand(uint32_t eventId, uint32_t target,
                                     uint32_t bufferId)
    : Command(eventId, "bindBuffer"), m_target(target), m_bufferId(bufferId) {}
void BindBufferCommand::execute() {
  glBindBuffer(m_target, getMappedHandleOr(ResourceKind::Buffer, m_bufferId));
}

BindBufferBaseCommand::BindBufferBaseCommand(uint32_t eventId, uint32_t target,
                                             uint32_t index, uint32_t bufferId)
    : Command(eventId, "bindBufferBase"), m_target(target), m_index(index),
      m_bufferId(bufferId) {}
void BindBufferBaseCommand::execute() {
  glBindBufferBase(m_target, m_index,
                   getMappedHandleOr(ResourceKind::Buffer, m_bufferId));
}

BindBufferRangeCommand::BindBufferRangeCommand(uint32_t eventId,
                                               uint32_t target, uint32_t index,
                                               uint32_t bufferId,
                                               uint64_t offset, uint64_t size)
    : Command(eventId, "bindBufferRange"), m_target(target), m_index(index),
      m_bufferId(bufferId), m_offset(offset), m_size(size) {}
void BindBufferRangeCommand::execute() {
  glBindBufferRange(
      m_target, m_index, getMappedHandleOr(ResourceKind::Buffer, m_bufferId),
      static_cast<GLintptr>(m_offset), static_cast<GLsizeiptr>(m_size));
}

BindTextureCommand::BindTextureCommand(uint32_t eventId, uint32_t target,
                                       uint32_t textureId, uint32_t unit)
    : Command(eventId, "bindTexture"), m_target(target), m_textureId(textureId),
      m_unit(unit) {}
void BindTextureCommand::execute() {
  glActiveTexture(GL_TEXTURE0 + m_unit);
  glBindTexture(m_target, getMappedHandleOr(ResourceKind::Texture, m_textureId));
}

ActiveTextureCommand::ActiveTextureCommand(uint32_t eventId, uint32_t unit)
    : Command(eventId, "activeTexture"), m_unit(unit) {}
void ActiveTextureCommand::execute() { glActiveTexture(GL_TEXTURE0 + m_unit); }

BindVertexArrayCommand::BindVertexArrayCommand(uint32_t eventId,
                                               uint32_t vertexArrayId)
    : Command(eventId, "bindVertexArray"), m_vertexArrayId(vertexArrayId) {}
void BindVertexArrayCommand::execute() {
  // Note: id 0 is the WebGL default VAO. The replay runs in a core profile,
  // which has no usable default VAO, so id 0 is backed by a real VAO handle
  // (see ResourceManager::ensureDefaultVertexArray). Use the mapping for id 0
  // too, falling back to 0 only if it somehow wasn't created.
  glBindVertexArray(getMappedHandleOr(ResourceKind::VertexArray, m_vertexArrayId));
}

VertexAttribPointerCommand::VertexAttribPointerCommand(
    uint32_t eventId, uint32_t attributeIndex, uint32_t componentCount,
    uint32_t componentType, bool normalized, uint32_t stride, uint32_t offset,
    uint32_t bufferId, std::string attributeName)
    : Command(eventId, "vertexAttribPointer"), m_attributeIndex(attributeIndex),
      m_componentCount(componentCount), m_componentType(componentType),
      m_normalized(normalized), m_stride(stride), m_offset(offset),
      m_bufferId(bufferId), m_attributeName(std::move(attributeName)) {}
void VertexAttribPointerCommand::execute() {
  glBindBuffer(GL_ARRAY_BUFFER,
               getMappedHandleOr(ResourceKind::Buffer, m_bufferId));
  glVertexAttribPointer(
      m_attributeIndex, static_cast<GLint>(m_componentCount), m_componentType,
      m_normalized ? GL_TRUE : GL_FALSE, static_cast<GLsizei>(m_stride),
      reinterpret_cast<const void *>(static_cast<uintptr_t>(m_offset)));
}

VertexAttribEnableCommand::VertexAttribEnableCommand(uint32_t eventId,
                                                     uint32_t attributeIndex,
                                                     bool enable)
    : Command(eventId,
              enable ? "enableVertexAttribArray" : "disableVertexAttribArray"),
      m_attributeIndex(attributeIndex), m_enable(enable) {}
void VertexAttribEnableCommand::execute() {
  if (m_enable)
    glEnableVertexAttribArray(m_attributeIndex);
  else
    glDisableVertexAttribArray(m_attributeIndex);
}

VertexAttribDivisorCommand::VertexAttribDivisorCommand(uint32_t eventId,
                                                       uint32_t attributeIndex,
                                                       uint32_t divisor)
    : Command(eventId, "vertexAttribDivisor"), m_attributeIndex(attributeIndex),
      m_divisor(divisor) {}
void VertexAttribDivisorCommand::execute() {
  glVertexAttribDivisor(m_attributeIndex, m_divisor);
}

// ---- draw commands ----

DrawElementsCommand::DrawElementsCommand(uint32_t eventId, uint32_t drawMode,
                                         uint32_t indexCount,
                                         uint32_t indexType,
                                         uint32_t indexOffset,
                                         uint32_t framebufferId)
    : Command(eventId, "drawElements"), m_drawMode(drawMode),
      m_indexCount(indexCount), m_indexType(indexType),
      m_indexOffset(indexOffset), m_framebufferId(framebufferId) {}
void DrawElementsCommand::execute() {
  if (m_framebufferId != 0) {
    if (hasMappedHandle(ResourceKind::Framebuffer, m_framebufferId))
      glBindFramebuffer(
          GL_FRAMEBUFFER,
          getMappedHandle(ResourceKind::Framebuffer, m_framebufferId));
  }
  uint32_t indexBuffer = 0;
  glDrawElements(
      m_drawMode, static_cast<GLsizei>(m_indexCount), m_indexType,
      reinterpret_cast<const void *>(static_cast<uintptr_t>(m_indexOffset)));
}

DrawArraysCommand::DrawArraysCommand(uint32_t eventId, uint32_t drawMode,
                                     uint32_t firstVertex, uint32_t vertexCount,
                                     uint32_t framebufferId)
    : Command(eventId, "drawArrays"), m_drawMode(drawMode),
      m_firstVertex(firstVertex), m_vertexCount(vertexCount),
      m_framebufferId(framebufferId) {}
void DrawArraysCommand::execute() {
  if (m_framebufferId != 0) {
    if (hasMappedHandle(ResourceKind::Framebuffer, m_framebufferId))
      glBindFramebuffer(
          GL_FRAMEBUFFER,
          getMappedHandle(ResourceKind::Framebuffer, m_framebufferId));
  }
  glDrawArrays(m_drawMode, static_cast<GLint>(m_firstVertex),
               static_cast<GLsizei>(m_vertexCount));
}

// ---- resource lifecycle ----

CreateResourceCommand::CreateResourceCommand(uint32_t eventId,
                                             ResourceKind kind,
                                             uint32_t resourceId,
                                             std::string name)
    : Command(eventId, std::move(name)), m_resourceKind(kind),
      m_resourceId(resourceId) {}
// Convert the Command-local ResourceKind to the global mapper ResourceKind.
static ::ResourceKind toMapperKind(CreateResourceCommand::ResourceKind kind) {
  switch (kind) {
  case CreateResourceCommand::ResourceKind::Buffer:      return ::ResourceKind::Buffer;
  case CreateResourceCommand::ResourceKind::Texture:     return ::ResourceKind::Texture;
  case CreateResourceCommand::ResourceKind::VertexArray: return ::ResourceKind::VertexArray;
  case CreateResourceCommand::ResourceKind::Framebuffer: return ::ResourceKind::Framebuffer;
  case CreateResourceCommand::ResourceKind::Shader:      return ::ResourceKind::Shader;
  case CreateResourceCommand::ResourceKind::Program:     return ::ResourceKind::Program;
  }
  return ::ResourceKind::Buffer;
}

void CreateResourceCommand::execute() {
  ::ResourceKind mappedKind = toMapperKind(m_resourceKind);

  // Idempotent: the replay loop re-runs the whole command list every frame, and
  // frame-start resources are already created during init. Reusing an existing
  // mapping avoids leaking a fresh GL object (and clobbering the mapping) on
  // every re-run.
  if (hasMappedHandle(mappedKind, m_resourceId))
    return;

  GLuint handle = 0;
  switch (m_resourceKind) {
  case ResourceKind::Buffer:
    glGenBuffers(1, &handle);
    break;
  case ResourceKind::Texture:
    glGenTextures(1, &handle);
    break;
  case ResourceKind::VertexArray:
    glGenVertexArrays(1, &handle);
    break;
  case ResourceKind::Framebuffer:
    glGenFramebuffers(1, &handle);
    break;
  case ResourceKind::Shader:
    return; // handled by ShaderSource
  case ResourceKind::Program:
    handle = glCreateProgram();
    break;
  }
  if (handle != 0)
    addMappedHandle(mappedKind, m_resourceId, handle);
}

DeleteResourceCommand::DeleteResourceCommand(uint32_t eventId,
                                             ResourceKind kind,
                                             uint32_t resourceId,
                                             std::string name)
    : Command(eventId, std::move(name)), m_resourceKind(kind),
      m_resourceId(resourceId) {}
void DeleteResourceCommand::execute() {
  switch (m_resourceKind) {
  case ResourceKind::Buffer:
    ResourceAllocator::deleteBuffer(m_resourceId);
    break;
  case ResourceKind::Texture:
    ResourceAllocator::deleteTexture(m_resourceId);
    break;
  case ResourceKind::VertexArray:
    ResourceAllocator::deleteVertexArray(m_resourceId);
    break;
  case ResourceKind::Framebuffer:
    ResourceAllocator::deleteFramebuffer(m_resourceId);
    break;
  case ResourceKind::Shader:
    ResourceAllocator::deleteShader(m_resourceId);
    break;
  case ResourceKind::Program:
    ResourceAllocator::deleteProgram(m_resourceId);
    break;
  }
}

// ---- data upload ----

BufferDataCommand::BufferDataCommand(uint32_t eventId, uint32_t target,
                                     uint32_t bufferId, uint64_t offset,
                                     CommandDataArgument data,
                                     std::string commandName)
    : Command(eventId, std::move(commandName)), m_target(target),
      m_bufferId(bufferId), m_offset(offset), m_data(std::move(data)) {}
void BufferDataCommand::execute() {
  if (!hasMappedHandle(ResourceKind::Buffer, m_bufferId))
    return;
  glBindBuffer(m_target, getMappedHandle(ResourceKind::Buffer, m_bufferId));

  // bufferSubData updates a region of an already-sized buffer; bufferData
  // (re)allocates. The previous code always called glBufferData and ignored the
  // offset, corrupting sub-updates.
  const bool isSubData = (getCommandName() == "bufferSubData");

  if (auto *ref = std::get_if<UploadReference>(&m_data)) {
    std::string fullPath = captureDirectory() + "/" + ref->m_uploadPath;
    std::ifstream file(fullPath, std::ios::binary | std::ios::ate);
    if (!file.is_open())
      return;
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<char> buffer(static_cast<size_t>(size));
    file.read(buffer.data(), size);
    if (isSubData)
      glBufferSubData(m_target, static_cast<GLintptr>(m_offset),
                      static_cast<GLsizeiptr>(buffer.size()), buffer.data());
    else
      glBufferData(m_target, static_cast<GLsizeiptr>(buffer.size()),
                   buffer.data(), GL_DYNAMIC_DRAW);
  } else if (auto *inlineData = std::get_if<UniformDataPayload>(&m_data)) {
    // Inline typed-array payload is stored as double; convert to float bytes.
    std::vector<float> floats(inlineData->m_data.begin(),
                              inlineData->m_data.end());
    GLsizeiptr byteSize = static_cast<GLsizeiptr>(floats.size() * sizeof(float));
    if (isSubData)
      glBufferSubData(m_target, static_cast<GLintptr>(m_offset), byteSize,
                      floats.data());
    else
      glBufferData(m_target, byteSize, floats.data(), GL_DYNAMIC_DRAW);
  }
  // std::monostate (e.g. "UploadSkipped") => no data to upload; no-op.
}

TextureImageCommand::TextureImageCommand(
    uint32_t eventId, uint32_t target, uint32_t mipmapLevel,
    uint32_t internalFormat, uint32_t width, uint32_t height, uint32_t format,
    uint32_t pixelType, uint32_t textureId, CommandDataArgument data,
    std::string commandName, uint32_t xOffset, uint32_t yOffset)
    : Command(eventId, std::move(commandName)), m_target(target),
      m_mipmapLevel(mipmapLevel), m_internalFormat(internalFormat),
      m_width(width), m_height(height), m_format(format),
      m_pixelType(pixelType), m_textureId(textureId), m_xOffset(xOffset),
      m_yOffset(yOffset), m_data(std::move(data)) {}
void TextureImageCommand::execute() {
  if (!hasMappedHandle(ResourceKind::Texture, m_textureId))
    return;
  glBindTexture(m_target, getMappedHandle(ResourceKind::Texture, m_textureId));
  const void *pixelData = nullptr;
  std::vector<char> buffer;
  if (auto *ref = std::get_if<UploadReference>(&m_data)) {
    std::string fullPath = captureDirectory() + "/" + ref->m_uploadPath;
    std::ifstream file(fullPath, std::ios::binary | std::ios::ate);
    if (file.is_open()) {
      std::streamsize size = file.tellg();
      file.seekg(0, std::ios::beg);
      buffer.resize(static_cast<size_t>(size));
      file.read(buffer.data(), size);
      pixelData = buffer.data();
    }
  }
  // texSubImage2D updates a sub-region (no internalFormat/border); texImage2D
  // (re)defines the whole level. The previous code always used glTexImage2D and
  // mis-read the sub-image args.
  if (getCommandName() == "texSubImage2D") {
    glTexSubImage2D(m_target, static_cast<GLint>(m_mipmapLevel),
                    static_cast<GLint>(m_xOffset), static_cast<GLint>(m_yOffset),
                    static_cast<GLsizei>(m_width),
                    static_cast<GLsizei>(m_height), m_format, m_pixelType,
                    pixelData);
  } else {
    glTexImage2D(m_target, static_cast<GLint>(m_mipmapLevel),
                 static_cast<GLint>(m_internalFormat),
                 static_cast<GLsizei>(m_width), static_cast<GLsizei>(m_height),
                 0, m_format, m_pixelType, pixelData);
  }
}

GenerateMipmapCommand::GenerateMipmapCommand(uint32_t eventId, uint32_t target,
                                             uint32_t textureId)
    : Command(eventId, "generateMipmap"), m_target(target),
      m_textureId(textureId) {}
void GenerateMipmapCommand::execute() {
  if (!hasMappedHandle(ResourceKind::Texture, m_textureId))
    return;
  glBindTexture(m_target, getMappedHandle(ResourceKind::Texture, m_textureId));
  glGenerateMipmap(m_target);
}

// ---- shader / program ----

ShaderSourceCommand::ShaderSourceCommand(uint32_t eventId, uint32_t shaderId,
                                         std::string source)
    : Command(eventId, "shaderSource"), m_shaderId(shaderId),
      m_source(std::move(source)) {}
void ShaderSourceCommand::execute() {
  if (!hasMappedHandle(ResourceKind::Shader, m_shaderId))
    return;
  GLuint handle = getMappedHandle(ResourceKind::Shader, m_shaderId);
  const char *src = m_source.c_str();
  glShaderSource(handle, 1, &src, nullptr);
  glCompileShader(handle);
}

AttachShaderCommand::AttachShaderCommand(uint32_t eventId, uint32_t programId,
                                         uint32_t shaderId)
    : Command(eventId, "attachShader"), m_programId(programId),
      m_shaderId(shaderId) {}
void AttachShaderCommand::execute() {
  if (!hasMappedHandle(ResourceKind::Program, m_programId) ||
      !hasMappedHandle(ResourceKind::Shader, m_shaderId))
    return;
  glAttachShader(getMappedHandle(ResourceKind::Program, m_programId),
                 getMappedHandle(ResourceKind::Shader, m_shaderId));
}

LinkProgramCommand::LinkProgramCommand(uint32_t eventId, uint32_t programId)
    : Command(eventId, "linkProgram"), m_programId(programId) {}
void LinkProgramCommand::execute() {
  if (!hasMappedHandle(ResourceKind::Program, m_programId))
    return;
  glLinkProgram(getMappedHandle(ResourceKind::Program, m_programId));
}

// ---- uniforms ----

UniformCommand::UniformCommand(uint32_t eventId, std::string uniformName,
                               uint32_t programId, bool valueOmitted,
                               std::string valueOmittedReason, bool isSnapshot,
                               CommandDataArgument data,
                               std::string commandName)
    : Command(eventId, std::move(commandName)),
      m_uniformName(std::move(uniformName)), m_programId(programId),
      m_valueOmitted(valueOmitted), m_isSnapshot(isSnapshot),
      m_valueOmittedReason(std::move(valueOmittedReason)),
      m_data(std::move(data)) {}
// Look up a uniform's real GL type (e.g. GL_FLOAT_MAT4) so we can pick the
// matching glUniform* setter. Snapshots dump every uniform as a flat float
// array under "uniform4fv", including matrices — so element count alone cannot
// tell a vec4[] from a mat4, and calling glUniform4fv on a mat4 yields
// GL_INVALID_OPERATION. Returns 0 if the type cannot be determined.
static GLenum queryUniformType(GLuint program, const std::string &name) {
  std::string baseName = name;
  size_t bracket = baseName.find('[');
  if (bracket != std::string::npos)
    baseName.erase(bracket); // "u_MVP[0]" -> "u_MVP"
  const char *namePtr = baseName.c_str();
  GLuint index = GL_INVALID_INDEX;
  glGetUniformIndices(program, 1, &namePtr, &index);
  if (index == GL_INVALID_INDEX)
    return 0;
  GLint type = 0;
  glGetActiveUniformsiv(program, 1, &index, GL_UNIFORM_TYPE, &type);
  return static_cast<GLenum>(type);
}

void UniformCommand::execute() {
  if (m_valueOmitted)
    return;
  if (!hasMappedHandle(ResourceKind::Program, m_programId))
    return;
  GLuint program = getMappedHandle(ResourceKind::Program, m_programId);
  glUseProgram(program);

  auto *uniform = std::get_if<UniformDataPayload>(&m_data);
  if (!uniform || uniform->m_data.empty())
    return;

  GLint location = glGetUniformLocation(program, m_uniformName.c_str());
  if (location < 0)
    return;

  // Captured values are stored as double; convert to float for the GL call.
  std::vector<GLfloat> values(uniform->m_data.begin(), uniform->m_data.end());
  const GLfloat *data = values.data();
  GLsizei count = static_cast<GLsizei>(values.size());

  switch (queryUniformType(program, m_uniformName)) {
  case GL_FLOAT_MAT2:
    glUniformMatrix2fv(location, count / 4, GL_FALSE, data);
    break;
  case GL_FLOAT_MAT3:
    glUniformMatrix3fv(location, count / 9, GL_FALSE, data);
    break;
  case GL_FLOAT_MAT4:
    glUniformMatrix4fv(location, count / 16, GL_FALSE, data);
    break;
  case GL_FLOAT_VEC2:
    glUniform2fv(location, count / 2, data);
    break;
  case GL_FLOAT_VEC3:
    glUniform3fv(location, count / 3, data);
    break;
  case GL_FLOAT_VEC4:
    glUniform4fv(location, count / 4, data);
    break;
  case GL_FLOAT:
    glUniform1fv(location, count, data);
    break;
  default:
    // Type introspection failed — fall back to a component-count guess.
    if (count % 4 == 0)
      glUniform4fv(location, count / 4, data);
    else if (count % 3 == 0)
      glUniform3fv(location, count / 3, data);
    else if (count % 2 == 0)
      glUniform2fv(location, count / 2, data);
    else
      glUniform1fv(location, count, data);
    break;
  }
}

UniformMatrixCommand::UniformMatrixCommand(uint32_t eventId,
                                           std::string uniformName,
                                           uint32_t programId, bool transpose,
                                           bool valueOmitted,
                                           CommandDataArgument data)
    : Command(eventId, "uniformMatrix4fv"),
      m_uniformName(std::move(uniformName)), m_programId(programId),
      m_transpose(transpose), m_valueOmitted(valueOmitted),
      m_data(std::move(data)) {}
void UniformMatrixCommand::execute() {
  if (m_valueOmitted)
    return;
  if (hasMappedHandle(ResourceKind::Program, m_programId))
    glUseProgram(getMappedHandle(ResourceKind::Program, m_programId));
  if (auto *uniform = std::get_if<UniformDataPayload>(&m_data)) {
    GLint location = glGetUniformLocation(
        getMappedHandle(ResourceKind::Program, m_programId),
        m_uniformName.c_str());
    if (location < 0)
      return;
    if (uniform->m_data.size() < 16)
      return;
    // Captured values are stored as double; convert to float for the GL call.
    std::vector<GLfloat> values(uniform->m_data.begin(), uniform->m_data.end());
    glUniformMatrix4fv(location,
                       static_cast<GLsizei>(values.size() / 16),
                       m_transpose ? GL_TRUE : GL_FALSE, values.data());
  }
}

UniformSamplerCommand::UniformSamplerCommand(uint32_t eventId,
                                             std::string uniformName,
                                             uint32_t programId,
                                             uint32_t textureUnit,
                                             bool valueOmitted)
    : Command(eventId, "uniform1i"), m_uniformName(std::move(uniformName)),
      m_programId(programId), m_textureUnit(textureUnit),
      m_valueOmitted(valueOmitted) {}
void UniformSamplerCommand::execute() {
  if (m_valueOmitted)
    return;
  if (!hasMappedHandle(ResourceKind::Program, m_programId))
    return;
  GLuint program = getMappedHandle(ResourceKind::Program, m_programId);
  glUseProgram(program);
  GLint location = glGetUniformLocation(program, m_uniformName.c_str());
  if (location >= 0)
    glUniform1i(location, static_cast<GLint>(m_textureUnit));
}

// ---- framebuffer ----

FramebufferTexture2DCommand::FramebufferTexture2DCommand(
    uint32_t eventId, uint32_t framebufferId, uint32_t attachment,
    uint32_t textureTarget, uint32_t textureId, uint32_t mipmapLevel)
    : Command(eventId, "framebufferTexture2D"), m_framebufferId(framebufferId),
      m_attachment(attachment), m_textureTarget(textureTarget),
      m_textureId(textureId), m_mipmapLevel(mipmapLevel) {}
void FramebufferTexture2DCommand::execute() {
  glBindFramebuffer(GL_FRAMEBUFFER,
                    getMappedHandleOr(ResourceKind::Framebuffer,
                                      m_framebufferId));
  glFramebufferTexture2D(GL_FRAMEBUFFER, m_attachment, m_textureTarget,
                         getMappedHandleOr(ResourceKind::Texture, m_textureId),
                         static_cast<GLint>(m_mipmapLevel));
}

BlitFramebufferCommand::BlitFramebufferCommand(
    uint32_t eventId, int32_t sourceX0, int32_t sourceY0, int32_t sourceX1,
    int32_t sourceY1, int32_t destinationX0, int32_t destinationY0,
    int32_t destinationX1, int32_t destinationY1, uint32_t mask,
    uint32_t filter)
    : Command(eventId, "blitFramebuffer"), m_sourceX0(sourceX0),
      m_sourceY0(sourceY0), m_sourceX1(sourceX1), m_sourceY1(sourceY1),
      m_destinationX0(destinationX0), m_destinationY0(destinationY0),
      m_destinationX1(destinationX1), m_destinationY1(destinationY1),
      m_mask(mask), m_filter(filter) {}
void BlitFramebufferCommand::execute() {
  glBlitFramebuffer(m_sourceX0, m_sourceY0, m_sourceX1, m_sourceY1,
                    m_destinationX0, m_destinationY0, m_destinationX1,
                    m_destinationY1, m_mask, m_filter);
}

// ---- no-op ----

NoOpCommand::NoOpCommand(uint32_t eventId, std::string name)
    : Command(eventId, std::move(name)) {}
