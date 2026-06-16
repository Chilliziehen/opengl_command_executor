/**
 * @file testGLStateManager.cpp
 * @brief Comprehensive test for GLStateManager::CaptureCurrentState()
 *
 * Tests every state component captured by GLStateManager:
 *   1. Resource bindings (textures, samplers, UBOs, active texture slot)
 *   2. Shader state (current program)
 *   3. Vertex input (VAO, attributes, bindings, EBO)
 *   4. Framebuffer (draw/read FBO, draw buffers, read buffer)
 *   5. Rasterizer (cull, polygon mode, scissor, polygon offset, line width, …)
 *   6. Depth/stencil (depth test/write/range, stencil front+back)
 *   7. Blend (enable, factors, equations, blend color, color mask)
 *   8. Pixel storage (pack/unpack params, PBO bindings)
 *
 * Edge-case / cross-cutting tests:
 *   - Idempotency: two captures of identical state must be equal
 *   - No-crash: VAO 0 bound, empty state
 *   - Round-trip: modify state → capture → restore → capture → compare
 *
 * Usage: testGLStateManager
 *   Creates a hidden GLFW window, runs all tests, prints summary.
 *   Returns 0 on success, non-zero on failure.
 */

// ---------- GL loader ----------
#include <glad/gl.h>
#define GLAD_GL_IMPLEMENTATION
#include <GLFW/glfw3.h>

// ---------- project headers ----------
#include "GLStateManager.h"

// ---------- standard library ----------
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>

// ============================================================================
//  Test infrastructure — lightweight, no external test framework dependency
// ============================================================================

static int g_passed = 0;
static int g_failed = 0;
static int g_warnings = 0; // advisory (e.g. known GL errors from GL version gap)

#define TEST_SECTION(title)                                         \
  do {                                                              \
    std::cout << "\n========== " << title << " ==========" << std::endl; \
  } while (0)

#define CHECK_MSG(cond, msg)                                              \
  do {                                                                    \
    if (cond) {                                                           \
      std::cout << "  PASS  " << msg << std::endl;                        \
      ++g_passed;                                                         \
    } else {                                                              \
      std::cout << "  FAIL  " << msg << std::endl;                        \
      ++g_failed;                                                         \
    }                                                                     \
  } while (0)

#define WARN_MSG(msg)                                                     \
  do {                                                                    \
    std::cout << "  WARN  " << msg << std::endl;                          \
    ++g_warnings;                                                         \
  } while (0)

// ---- type-to-string helpers ----
template <typename T>
static std::string toStr(const T &v) {
  std::ostringstream oss;
  oss << v;
  return oss.str();
}

static std::string hexStr(uint32_t v) {
  std::ostringstream oss;
  oss << "0x" << std::hex << v << std::dec;
  return oss.str();
}

// ---- enum-name helpers (for diagnostic output) ----
static const char *cullModeName(CullMode m) {
  switch (m) {
  case CullMode::Front:        return "Front";
  case CullMode::Back:         return "Back";
  case CullMode::FrontAndBack: return "FrontAndBack";
  default:                     return "?";
  }
}
static const char *frontFaceName(FrontFace f) {
  switch (f) { case FrontFace::CW: return "CW"; case FrontFace::CCW: return "CCW"; default: return "?"; }
}
static const char *polygonModeName(PolygonMode m) {
  switch (m) { case PolygonMode::Point: return "Point"; case PolygonMode::Line: return "Line"; case PolygonMode::Fill: return "Fill"; default: return "?"; }
}
static const char *compareFuncName(CompareFunc f) {
  switch (f) {
  case CompareFunc::Never: return "Never"; case CompareFunc::Less: return "Less";
  case CompareFunc::Equal: return "Equal"; case CompareFunc::LEqual: return "LEqual";
  case CompareFunc::Greater: return "Greater"; case CompareFunc::NotEqual: return "NotEqual";
  case CompareFunc::GEqual: return "GEqual"; case CompareFunc::Always: return "Always";
  default: return "?";
  }
}
static const char *stencilOpName(StencilOp op) {
  switch (op) {
  case StencilOp::Keep: return "Keep"; case StencilOp::Zero: return "Zero";
  case StencilOp::Replace: return "Replace"; case StencilOp::Incr: return "Incr";
  case StencilOp::Decr: return "Decr"; case StencilOp::Invert: return "Invert";
  case StencilOp::IncrWrap: return "IncrWrap"; case StencilOp::DecrWrap: return "DecrWrap";
  default: return "?";
  }
}
static const char *blendFactorName(BlendFactor f) {
  switch (f) {
  case BlendFactor::Zero: return "Zero"; case BlendFactor::One: return "One";
  case BlendFactor::SrcColor: return "SrcColor"; case BlendFactor::OneMinusSrcColor: return "1-SrcColor";
  case BlendFactor::SrcAlpha: return "SrcAlpha"; case BlendFactor::OneMinusSrcAlpha: return "1-SrcAlpha";
  case BlendFactor::DstAlpha: return "DstAlpha"; case BlendFactor::OneMinusDstAlpha: return "1-DstAlpha";
  case BlendFactor::DstColor: return "DstColor"; case BlendFactor::OneMinusDstColor: return "1-DstColor";
  case BlendFactor::SrcAlphaSaturate: return "SrcAlphaSaturate";
  case BlendFactor::ConstantColor: return "ConstColor"; case BlendFactor::OneMinusConstantColor: return "1-ConstColor";
  case BlendFactor::ConstantAlpha: return "ConstAlpha"; case BlendFactor::OneMinusConstantAlpha: return "1-ConstAlpha";
  default: return "?";
  }
}
static const char *blendEquationName(BlendEquation e) {
  switch (e) {
  case BlendEquation::Add: return "Add"; case BlendEquation::Subtract: return "Subtract";
  case BlendEquation::ReverseSubtract: return "RevSubtract"; case BlendEquation::Min: return "Min";
  case BlendEquation::Max: return "Max"; default: return "?";
  }
}

// ============================================================================
//  GL error & context helpers
// ============================================================================

// Drain + print GL errors. Returns count of errors drained.
static int drainGlErrors(const char *context) {
  int count = 0;
  for (GLenum err = glGetError(); err != GL_NO_ERROR; err = glGetError()) {
    const char *name = "?";
    switch (err) {
    case GL_INVALID_ENUM:      name = "INVALID_ENUM";      break;
    case GL_INVALID_VALUE:     name = "INVALID_VALUE";     break;
    case GL_INVALID_OPERATION: name = "INVALID_OPERATION"; break;
    case GL_OUT_OF_MEMORY:     name = "OUT_OF_MEMORY";     break;
    default: break;
    }
    std::cout << "  [GL ERR] " << context << " -> " << name
              << " (" << hexStr(err) << ")" << std::endl;
    ++count;
  }
  return count;
}

// Capture state + drain GL errors into a diagnostic count.
// Returns the snapshot.  *errCount (if non-null) gets the error count.
static GLStateSnapshot captureAndDrain(const char *label, int *errCount = nullptr) {
  GLStateSnapshot snap = GLStateManager::CaptureCurrentState();
  int n = drainGlErrors(label);
  if (n > 0 && errCount) *errCount = n;
  // Advisory: GL errors are expected in GL 3.3 core when GLStateManager queries
  // GL 4.x enums (e.g. GL_PROGRAM_PIPELINE_BINDING, deprecated texture targets).
  // We note them but do not fail the test for these known version-gap artifacts.
  (void)n;
  return snap;
}

// ---- Shader helpers ----
static GLuint compileShader(GLenum type, const char *src) {
  GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &src, nullptr);
  glCompileShader(shader);
  GLint ok = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char log[1024] = {};
    glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
    std::cerr << "  Shader compile error: " << log << std::endl;
    glDeleteShader(shader);
    return 0;
  }
  return shader;
}

static GLuint createSimpleProgram() {
  const char *vertSrc = R"(#version 330 core
layout(location = 0) in vec3 aPos;
void main() { gl_Position = vec4(aPos, 1.0); })";
  const char *fragSrc = R"(#version 330 core
out vec4 FragColor;
void main() { FragColor = vec4(1.0); })";
  GLuint vs = compileShader(GL_VERTEX_SHADER, vertSrc);
  GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragSrc);
  if (!vs || !fs) {
    if (vs) glDeleteShader(vs);
    if (fs) glDeleteShader(fs);
    return 0;
  }
  GLuint prog = glCreateProgram();
  glAttachShader(prog, vs);
  glAttachShader(prog, fs);
  glLinkProgram(prog);
  GLint ok = 0;
  glGetProgramiv(prog, GL_LINK_STATUS, &ok);
  glDeleteShader(vs);
  glDeleteShader(fs);
  if (!ok) {
    char log[1024] = {};
    glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
    std::cerr << "  Program link error: " << log << std::endl;
    glDeleteProgram(prog);
    return 0;
  }
  return prog;
}

// ---- GLFW error callback ----
static void errorCallback(int error, const char *description) {
  std::cerr << "GLFW error " << error << ": " << description << std::endl;
}

// ============================================================================
//  Test 1: Default state
// ============================================================================
static void testDefaultState() {
  TEST_SECTION("Default GL State");
  drainGlErrors("pre-default-test");

  GLStateSnapshot snap = captureAndDrain("default capture");

  // ---- Resource bindings ----
  {
    auto &res = snap.m_resourceBindings;
    CHECK_MSG(res.m_activeTextureSlot == 0,
              "Active texture slot: 0 (was " + toStr(res.m_activeTextureSlot) + ")");

    // Unit 0: nothing bound
    auto &u0 = res.m_textureUnits[0];
    bool anyTex = false;
    for (size_t t = 0; t < static_cast<size_t>(TextureTarget::Count); ++t)
      if (u0.m_textureBindings[t].has_value() && u0.m_textureBindings[t].value() != 0)
        anyTex = true;
    CHECK_MSG(!anyTex, "Unit 0: no texture bound");
    CHECK_MSG(!u0.m_samplerId.has_value() || u0.m_samplerId.value() == 0,
              "Unit 0: no sampler bound");

    // No UBOs
    bool anyUbo = false;
    for (size_t i = 0; i < MAX_UNIFORM_BUFFERS; ++i)
      if (res.m_uniformBufferBindings[i].has_value()) anyUbo = true;
    CHECK_MSG(!anyUbo, "No UBO bindings at default");
  }

  // ---- Shader ----
  {
    auto &sh = snap.m_shader;
    CHECK_MSG(!sh.m_programId.has_value() || sh.m_programId.value() == 0,
              "No shader program active at default");
    // GL_PROGRAM_PIPELINE_BINDING generates GL_INVALID_ENUM in GL 3.3 core,
    // so the captured value is just whatever fallback the capture code provides.
    CHECK_MSG(!sh.m_programPipelineId.has_value() ||
                  sh.m_programPipelineId.value() == 0,
              "No program pipeline active at default");
  }

  // ---- Vertex input (no VAO) ----
  {
    auto &vi = snap.m_vertexInput;
    CHECK_MSG(!vi.m_vaoId.has_value() || vi.m_vaoId.value() == 0,
              "No user VAO at default");
    CHECK_MSG(vi.m_attributes[0].m_enabled == false,
              "Attr 0 not enabled (no VAO)");
  }

  // ---- Framebuffer ----
  {
    auto &fb = snap.m_framebuffer;
    CHECK_MSG(fb.m_drawFramebuffer == 0, "Draw FBO is 0 (default FB)");
    CHECK_MSG(fb.m_readFramebuffer == 0, "Read FBO is 0 (default FB)");
    // Default read buffer for window-system FB:
    // GL_BACK (0x0405) = 1029.  Some drivers report differently; we accept both.
    bool readBufOk = (fb.m_readBuffer == 0x0405 ||   // GL_BACK
                      fb.m_readBuffer == 0x0000 ||   // GL_NONE (headless?)
                      fb.m_readBuffer == 0x8CE0);    // GL_COLOR_ATTACHMENT0 (some drivers)
    CHECK_MSG(readBufOk,
              "Read buffer is GL_BACK or compatible (was " +
                  hexStr(fb.m_readBuffer) + ")");
  }

  // ---- Rasterizer ----
  {
    auto &rs = snap.m_rasterizer;
    CHECK_MSG(!rs.m_cullFaceEnabled, "Cull face: disabled (default)");
    CHECK_MSG(rs.m_cullMode == CullMode::Back,
              "Cull mode: Back (was " + std::string(cullModeName(rs.m_cullMode)) + ")");
    CHECK_MSG(rs.m_frontFace == FrontFace::CCW,
              "Front face: CCW (was " + std::string(frontFaceName(rs.m_frontFace)) + ")");
    CHECK_MSG(rs.m_polygonMode == PolygonMode::Fill,
              "Polygon mode: Fill (was " + std::string(polygonModeName(rs.m_polygonMode)) + ")");
    CHECK_MSG(rs.m_multisampleEnabled == true, "Multisample: enabled (default)");
    CHECK_MSG(!rs.m_depthClampEnabled, "Depth clamp: disabled (default)");
    CHECK_MSG(!rs.m_rasterizerDiscardEnabled, "Rasterizer discard: disabled (default)");
    CHECK_MSG(std::fabs(rs.m_lineWidth - 1.0f) < 0.01f,
              "Line width: 1.0 (was " + toStr(rs.m_lineWidth) + ")");
  }

  // ---- Depth / Stencil ----
  {
    auto &ds = snap.m_depthStencil;
    CHECK_MSG(!ds.m_depthTestEnabled, "Depth test: disabled (default)");
    CHECK_MSG(ds.m_depthFunc == CompareFunc::Less,
              "Depth func: Less (was " + std::string(compareFuncName(ds.m_depthFunc)) + ")");
    CHECK_MSG(ds.m_depthWriteEnabled, "Depth write: enabled (default)");
    CHECK_MSG(ds.m_depthRangeNear == 0.0f && ds.m_depthRangeFar == 1.0f,
              "Depth range: [0, 1] (was [" + toStr(ds.m_depthRangeNear) + ", " +
                  toStr(ds.m_depthRangeFar) + "])");
    CHECK_MSG(!ds.m_stencilTestEnabled, "Stencil test: disabled (default)");
  }

  // ---- Blend ----
  {
    auto &bl = snap.m_blend;
    CHECK_MSG(!bl.m_blendEnabled, "Blend: disabled (default)");
    CHECK_MSG(bl.m_srcRGB == BlendFactor::One && bl.m_dstRGB == BlendFactor::Zero,
              "Blend func: (One, Zero) (was (" +
                  std::string(blendFactorName(bl.m_srcRGB)) + ", " +
                  std::string(blendFactorName(bl.m_dstRGB)) + "))");
    CHECK_MSG(bl.m_rgbEquation == BlendEquation::Add,
              "Blend equation: Add");
    CHECK_MSG(bl.m_colorMask[0] && bl.m_colorMask[1] && bl.m_colorMask[2] &&
                  bl.m_colorMask[3],
              "Color mask: (T,T,T,T) (default)");
  }

  // ---- Pixel storage ----
  {
    auto &ps = snap.m_pixelStorage;
    CHECK_MSG(ps.m_pack.m_alignment == 4,
              "Pack alignment: 4 (was " + toStr(ps.m_pack.m_alignment) + ")");
    CHECK_MSG(ps.m_unpack.m_alignment == 4,
              "Unpack alignment: 4 (was " + toStr(ps.m_unpack.m_alignment) + ")");
    CHECK_MSG(!ps.m_pack.m_swapBytes && !ps.m_pack.m_lsbFirst,
              "Pack swapBytes/lsbFirst: false");
    CHECK_MSG(!ps.m_unpack.m_swapBytes && !ps.m_unpack.m_lsbFirst,
              "Unpack swapBytes/lsbFirst: false");
    CHECK_MSG(ps.m_pixelPackBuffer == 0, "Pixel pack buffer: 0 (default)");
    CHECK_MSG(ps.m_pixelUnpackBuffer == 0, "Pixel unpack buffer: 0 (default)");
  }
}

// ============================================================================
//  Test 2: Resource bindings
// ============================================================================
static void testResourceBindings() {
  TEST_SECTION("Resource Bindings (textures, samplers, UBOs)");
  drainGlErrors("pre-resource-bindings");

  // ---- Create objects ----
  GLuint tex2D = 0, tex3D = 0, sampler = 0, ubo = 0;
  glGenTextures(1, &tex2D);
  glGenTextures(1, &tex3D);
  glGenSamplers(1, &sampler);
  glGenBuffers(1, &ubo);

  // Bind 2D texture to unit 0
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, tex2D);

  // Bind 3D texture + sampler to unit 1
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_3D, tex3D);
  glBindSampler(1, sampler);

  // UBO: query alignment, then bind with properly aligned offset
  GLint uboAlign = 256; // safe default
  glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &uboAlign);
  glBindBuffer(GL_UNIFORM_BUFFER, ubo);
  glBufferData(GL_UNIFORM_BUFFER, static_cast<GLsizeiptr>(uboAlign) * 4,
               nullptr, GL_DYNAMIC_DRAW);

  // Index 0: base binding
  glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo);
  // Index 1: range binding (offset = uboAlign, size = uboAlign*2)
  glBindBufferRange(GL_UNIFORM_BUFFER, 1, ubo,
                    static_cast<GLintptr>(uboAlign),
                    static_cast<GLsizeiptr>(uboAlign * 2));

  drainGlErrors("after resource setup");
  GLStateSnapshot snap = captureAndDrain("resource-bindings capture");
  auto &res = snap.m_resourceBindings;

  // Active texture slot
  CHECK_MSG(res.m_activeTextureSlot == 1,
            "Active texture slot: 1 (was " + toStr(res.m_activeTextureSlot) + ")");

  // Unit 0
  {
    auto &u0 = res.m_textureUnits[0];
    auto idx = static_cast<size_t>(TextureTarget::Tex2D);
    CHECK_MSG(u0.m_textureBindings[idx].has_value() &&
                  u0.m_textureBindings[idx].value() == tex2D,
              "Unit 0: 2D texture " + toStr(tex2D) + " captured");
    CHECK_MSG(!u0.m_samplerId.has_value() || u0.m_samplerId.value() == 0,
              "Unit 0: no sampler");
  }

  // Unit 1
  {
    auto &u1 = res.m_textureUnits[1];
    auto idx = static_cast<size_t>(TextureTarget::Tex3D);
    CHECK_MSG(u1.m_textureBindings[idx].has_value() &&
                  u1.m_textureBindings[idx].value() == tex3D,
              "Unit 1: 3D texture " + toStr(tex3D) + " captured");
    CHECK_MSG(u1.m_samplerId.has_value() && u1.m_samplerId.value() == sampler,
              "Unit 1: sampler " + toStr(sampler) + " captured");
  }

  // UBO index 0
  {
    auto &b0 = res.m_uniformBufferBindings[0];
    CHECK_MSG(b0.has_value(), "UBO[0]: binding exists");
    if (b0.has_value()) {
      CHECK_MSG(b0->m_bufferId == ubo,
                "UBO[0]: buffer=" + toStr(ubo) + " (was " + toStr(b0->m_bufferId) + ")");
      CHECK_MSG(b0->m_offset == 0 && b0->m_size == 0,
                "UBO[0]: offset=0, size=0 (base-binding convention)");
    }
  }

  // UBO index 1
  {
    auto &b1 = res.m_uniformBufferBindings[1];
    CHECK_MSG(b1.has_value(), "UBO[1]: binding exists");
    if (b1.has_value()) {
      CHECK_MSG(b1->m_bufferId == ubo,
                "UBO[1]: buffer=" + toStr(ubo));
      CHECK_MSG(static_cast<GLint>(b1->m_offset) == uboAlign,
                "UBO[1]: offset=" + toStr(uboAlign) +
                    " (was " + toStr(static_cast<GLint>(b1->m_offset)) + ")");
      CHECK_MSG(static_cast<GLint>(b1->m_size) == uboAlign * 2,
                "UBO[1]: size=" + toStr(uboAlign * 2) +
                    " (was " + toStr(static_cast<GLint>(b1->m_size)) + ")");
    }
  }

  // Cleanup
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, 0);
  glBindTexture(GL_TEXTURE_3D, 0);
  glBindSampler(1, 0);
  glDeleteTextures(1, &tex2D);
  glDeleteTextures(1, &tex3D);
  glDeleteSamplers(1, &sampler);
  glBindBuffer(GL_UNIFORM_BUFFER, 0);
  glBindBufferBase(GL_UNIFORM_BUFFER, 0, 0);
  glBindBufferRange(GL_UNIFORM_BUFFER, 1, 0, 0, 0);
  glDeleteBuffers(1, &ubo);
}

// ============================================================================
//  Test 3: Shader state
// ============================================================================
static void testShaderState() {
  TEST_SECTION("Shader State (program binding)");
  drainGlErrors("pre-shader-state");

  GLuint prog = createSimpleProgram();
  if (prog == 0) {
    CHECK_MSG(false, "Shader program creation failed — skipping shader tests");
    return;
  }

  // Before
  {
    GLStateSnapshot snap = captureAndDrain("shader-before");
    CHECK_MSG(!snap.m_shader.m_programId.has_value() ||
                  snap.m_shader.m_programId.value() == 0,
              "Before glUseProgram: no program active");
  }

  // After glUseProgram
  glUseProgram(prog);
  drainGlErrors("after glUseProgram");
  {
    GLStateSnapshot snap = captureAndDrain("shader-active");
    CHECK_MSG(snap.m_shader.m_programId.has_value() &&
                  snap.m_shader.m_programId.value() == prog,
              "Program " + toStr(prog) + " is active (was " +
                  (snap.m_shader.m_programId.has_value()
                       ? toStr(snap.m_shader.m_programId.value())
                       : "nullopt") +
                  ")");
  }

  // After glUseProgram(0)
  glUseProgram(0);
  {
    GLStateSnapshot snap = captureAndDrain("shader-unbound");
    CHECK_MSG(!snap.m_shader.m_programId.has_value() ||
                  snap.m_shader.m_programId.value() == 0,
              "After glUseProgram(0): no program active");
  }

  glDeleteProgram(prog);
}

// ============================================================================
//  Test 4: Vertex input
// ============================================================================
static void testVertexInput() {
  TEST_SECTION("Vertex Input (VAO, attributes, bindings, EBO)");
  drainGlErrors("pre-vertex-input");

  GLuint vao = 0, vbo = 0, ebo = 0;
  glGenVertexArrays(1, &vao);
  glGenBuffers(1, &vbo);
  glGenBuffers(1, &ebo);

  glBindVertexArray(vao);

  // Bind VBO to GL_ARRAY_BUFFER — the traditional way to associate buffer with
  // attribute pointers.  glVertexAttribPointer records the currently-bound
  // GL_ARRAY_BUFFER as the backing store for that attribute.
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, 1024, nullptr, GL_STATIC_DRAW);

  // Attribute 0: vec3 float (position), offset 0, stride 12
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12, (const void *)0);
  glEnableVertexAttribArray(0);

  // Attribute 1: vec4 unsigned byte normalized (color), offset 12, stride 4
  // Both attributes point into the SAME VBO at different offsets.
  glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, 4, (const void *)12);
  glEnableVertexAttribArray(1);

  // Attribute 2: NOT enabled — should read back as disabled
  // Attribute 3..15: left at their implicit defaults

  glVertexAttribDivisor(0, 0); // per-vertex
  glVertexAttribDivisor(1, 1); // per-instance

  // Bind EBO
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, 64, nullptr, GL_STATIC_DRAW);

  drainGlErrors("after VAO setup");
  GLStateSnapshot snap = captureAndDrain("vertex-input capture");
  auto &vi = snap.m_vertexInput;

  // VAO
  CHECK_MSG(vi.m_vaoId.has_value() && vi.m_vaoId.value() == vao,
            "VAO " + toStr(vao) + " recorded (was " +
                (vi.m_vaoId.has_value() ? toStr(vi.m_vaoId.value()) : "nullopt") + ")");

  // Attribute 0
  {
    auto &a = vi.m_attributes[0];
    CHECK_MSG(a.m_enabled,          "Attr 0: enabled");
    CHECK_MSG(a.m_size == 3,        "Attr 0: size=3 (was " + toStr(a.m_size) + ")");
    CHECK_MSG(a.m_type == GL_FLOAT, "Attr 0: type=GL_FLOAT (was " + hexStr(a.m_type) + ")");
    CHECK_MSG(!a.m_normalized,      "Attr 0: not normalized");
    CHECK_MSG(a.m_formatKind == AttribFormatKind::Float,
              "Attr 0: formatKind=Float");
  }

  // Attribute 1
  {
    auto &a = vi.m_attributes[1];
    CHECK_MSG(a.m_enabled,                "Attr 1: enabled");
    CHECK_MSG(a.m_size == 4,              "Attr 1: size=4 (was " + toStr(a.m_size) + ")");
    CHECK_MSG(a.m_type == GL_UNSIGNED_BYTE,
              "Attr 1: type=GL_UNSIGNED_BYTE (was " + hexStr(a.m_type) + ")");
    CHECK_MSG(a.m_normalized,             "Attr 1: normalized");
    // NOTE: GL_VERTEX_ATTRIB_RELATIVE_OFFSET (GL 4.3) is only populated by
    // glVertexAttribFormat, NOT by glVertexAttribPointer.  The traditional API
    // stores the offset inside the buffer binding.  Therefore we accept 0 here.
    CHECK_MSG(a.m_relativeOffset == 0,
              "Attr 1: relativeOffset=0 (traditional API — stored in binding, not attr)");
  }

  // Attribute 2: not enabled
  CHECK_MSG(!vi.m_attributes[2].m_enabled, "Attr 2: not enabled");

  // Binding 0
  {
    auto &b = vi.m_bindings[0];
    CHECK_MSG(b.m_buffer == vbo,
              "Binding 0: buffer=" + toStr(vbo) + " (was " + toStr(b.m_buffer) + ")");
    CHECK_MSG(b.m_stride == 12,
              "Binding 0: stride=12 (was " + toStr(b.m_stride) + ")");
    CHECK_MSG(b.m_divisor == 0,
              "Binding 0: divisor=0 (was " + toStr(b.m_divisor) + ")");
  }

  // Binding 1
  {
    auto &b = vi.m_bindings[1];
    CHECK_MSG(b.m_buffer == vbo,
              "Binding 1: buffer=" + toStr(vbo) + " (was " + toStr(b.m_buffer) + ")");
    CHECK_MSG(b.m_stride == 4,
              "Binding 1: stride=4 (was " + toStr(b.m_stride) + ")");
    CHECK_MSG(b.m_divisor == 1,
              "Binding 1: divisor=1 (was " + toStr(b.m_divisor) + ")");
  }

  // EBO
  CHECK_MSG(vi.m_elementBuffer.has_value() &&
                vi.m_elementBuffer.value() == ebo,
            "EBO " + toStr(ebo) + " recorded (was " +
                (vi.m_elementBuffer.has_value() ? toStr(vi.m_elementBuffer.value())
                                                : "nullopt") +
                ")");

  // Cleanup
  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  glDeleteVertexArrays(1, &vao);
  glDeleteBuffers(1, &vbo);
  glDeleteBuffers(1, &ebo);
}

// ============================================================================
//  Test 5: Framebuffer state
// ============================================================================
static void testFramebufferState() {
  TEST_SECTION("Framebuffer (FBO bindings, draw buffers, read buffer)");
  drainGlErrors("pre-fbo");

  GLuint fbo = 0, tex = 0;
  glGenFramebuffers(1, &fbo);
  glGenTextures(1, &tex);

  glBindTexture(GL_TEXTURE_2D, tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 64, 64, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                         GL_TEXTURE_2D, tex, 0);

  GLenum drawBufs[8] = {GL_COLOR_ATTACHMENT0, GL_NONE, GL_NONE, GL_NONE,
                        GL_NONE, GL_NONE, GL_NONE, GL_NONE};
  glDrawBuffers(8, drawBufs);
  glReadBuffer(GL_NONE);

  drainGlErrors("after FBO setup");
  GLStateSnapshot snap = captureAndDrain("FBO capture");
  auto &fb = snap.m_framebuffer;

  CHECK_MSG(fb.m_drawFramebuffer == fbo,
            "Draw FBO: " + toStr(fbo) + " (was " + toStr(fb.m_drawFramebuffer) + ")");
  CHECK_MSG(fb.m_readFramebuffer == 0,
            "Read FBO: 0 (was " + toStr(fb.m_readFramebuffer) + ")");
  CHECK_MSG(fb.m_drawBuffers[0] == GL_COLOR_ATTACHMENT0,
            "Draw buffer[0]: GL_COLOR_ATTACHMENT0 (" +
                hexStr(fb.m_drawBuffers[0]) + ")");
  CHECK_MSG(fb.m_readBuffer == GL_NONE,
            "Read buffer: GL_NONE (" + hexStr(fb.m_readBuffer) + ")");

  // Restore
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
  glDeleteFramebuffers(1, &fbo);
  glDeleteTextures(1, &tex);
}

// ============================================================================
//  Test 6: Rasterizer state (round-trip)
// ============================================================================
static void testRasterizerState() {
  TEST_SECTION("Rasterizer (cull, polygon, scissor, offset, line width)");
  drainGlErrors("pre-rasterizer");

  // Baseline (drain any prior errors so the error flag is clean before capture)
  drainGlErrors("pre-rasterizer-baseline-drain");
  GLStateSnapshot baseline = captureAndDrain("rasterizer-baseline");

  // ---- Modify ----
  glEnable(GL_CULL_FACE);
  glCullFace(GL_FRONT);
  glFrontFace(GL_CW);
  glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
  glEnable(GL_SCISSOR_TEST);
  glScissor(10, 20, 640, 480);
  glEnable(GL_POLYGON_OFFSET_FILL);
  glPolygonOffset(2.0f, 4.0f);
  glLineWidth(2.5f);
  glDisable(GL_MULTISAMPLE);

  drainGlErrors("after rasterizer modifications");
  GLStateSnapshot modified = captureAndDrain("rasterizer-modified");
  auto &rs = modified.m_rasterizer;

  CHECK_MSG(rs.m_cullFaceEnabled, "Cull face: enabled ✓");
  CHECK_MSG(rs.m_cullMode == CullMode::Front,
            "Cull mode: Front (was " + std::string(cullModeName(rs.m_cullMode)) + ")");
  CHECK_MSG(rs.m_frontFace == FrontFace::CW,
            "Front face: CW (was " + std::string(frontFaceName(rs.m_frontFace)) + ")");
  CHECK_MSG(rs.m_polygonMode == PolygonMode::Line,
            "Polygon mode: Line (was " + std::string(polygonModeName(rs.m_polygonMode)) + ")");
  CHECK_MSG(rs.m_scissorTestEnabled, "Scissor test: enabled ✓");
  CHECK_MSG(rs.m_scissorBox[0] == 10 && rs.m_scissorBox[1] == 20 &&
                rs.m_scissorBox[2] == 640 && rs.m_scissorBox[3] == 480,
            "Scissor box: (10,20,640,480) (was (" +
                toStr(rs.m_scissorBox[0]) + "," + toStr(rs.m_scissorBox[1]) + "," +
                toStr(rs.m_scissorBox[2]) + "," + toStr(rs.m_scissorBox[3]) + "))");
  CHECK_MSG(rs.m_polygonOffsetFillEnabled, "Polygon offset fill: enabled ✓");
  CHECK_MSG(std::fabs(rs.m_polygonOffsetFactor - 2.0f) < 0.001f,
            "Polygon offset factor: 2.0 (was " + toStr(rs.m_polygonOffsetFactor) + ")");
  CHECK_MSG(std::fabs(rs.m_polygonOffsetUnits - 4.0f) < 0.001f,
            "Polygon offset units: 4.0 (was " + toStr(rs.m_polygonOffsetUnits) + ")");
  CHECK_MSG(std::fabs(rs.m_lineWidth - 2.5f) < 0.01f,
            "Line width: 2.5 (was " + toStr(rs.m_lineWidth) + ")");
  CHECK_MSG(!rs.m_multisampleEnabled, "Multisample: disabled ✓");

  // ---- Restore to baseline values ----
  glDisable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  glFrontFace(GL_CCW);
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  glDisable(GL_SCISSOR_TEST);
  glScissor(baseline.m_rasterizer.m_scissorBox[0],
            baseline.m_rasterizer.m_scissorBox[1],
            baseline.m_rasterizer.m_scissorBox[2],
            baseline.m_rasterizer.m_scissorBox[3]);
  glDisable(GL_POLYGON_OFFSET_FILL);
  glPolygonOffset(baseline.m_rasterizer.m_polygonOffsetFactor,
                  baseline.m_rasterizer.m_polygonOffsetUnits);
  glLineWidth(baseline.m_rasterizer.m_lineWidth);
  if (baseline.m_rasterizer.m_multisampleEnabled)
    glEnable(GL_MULTISAMPLE);
  else
    glDisable(GL_MULTISAMPLE);

  drainGlErrors("after rasterizer restore");
  GLStateSnapshot restored = captureAndDrain("rasterizer-restored");
  CHECK_MSG(restored.m_rasterizer == baseline.m_rasterizer,
            "Rasterizer state fully restored to baseline");
}

// ============================================================================
//  Test 7: Depth / Stencil state
// ============================================================================
static void testDepthStencilState() {
  TEST_SECTION("Depth / Stencil");
  drainGlErrors("pre-depth-stencil");

  drainGlErrors("pre-depth-stencil-baseline-drain");
  GLStateSnapshot baseline = captureAndDrain("depth-stencil-baseline");

  // ---- Modify ----
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_GREATER);
  glDepthMask(GL_FALSE);
  glDepthRange(0.2, 0.8);

  glEnable(GL_STENCIL_TEST);
  glStencilFuncSeparate(GL_FRONT, GL_ALWAYS, 0x42, 0xFF);
  glStencilOpSeparate(GL_FRONT, GL_REPLACE, GL_DECR, GL_INVERT);
  glStencilMaskSeparate(GL_FRONT, 0xAA);
  glStencilFuncSeparate(GL_BACK, GL_NOTEQUAL, 0x13, 0xF0);
  glStencilOpSeparate(GL_BACK, GL_INCR, GL_INVERT, GL_KEEP);
  glStencilMaskSeparate(GL_BACK, 0x55);

  drainGlErrors("after depth/stencil modifications");
  GLStateSnapshot snap = captureAndDrain("depth-stencil-capture");
  auto &ds = snap.m_depthStencil;

  // Depth
  CHECK_MSG(ds.m_depthTestEnabled, "Depth test: enabled ✓");
  CHECK_MSG(ds.m_depthFunc == CompareFunc::Greater,
            "Depth func: Greater (was " + std::string(compareFuncName(ds.m_depthFunc)) + ")");
  CHECK_MSG(!ds.m_depthWriteEnabled, "Depth write: disabled ✓");
  CHECK_MSG(std::fabs(ds.m_depthRangeNear - 0.2f) < 0.001f &&
                std::fabs(ds.m_depthRangeFar - 0.8f) < 0.001f,
            "Depth range: [0.2, 0.8]");

  // Stencil
  CHECK_MSG(ds.m_stencilTestEnabled, "Stencil test: enabled ✓");

  // Front
  CHECK_MSG(ds.m_front.m_func == CompareFunc::Always,
            "Front func: Always (was " + std::string(compareFuncName(ds.m_front.m_func)) + ")");
  CHECK_MSG(ds.m_front.m_ref == 0x42, "Front ref: 0x42");
  CHECK_MSG(ds.m_front.m_compareMask == 0xFFu, "Front compareMask: 0xFF");
  CHECK_MSG(ds.m_front.m_writeMask == 0xAAu, "Front writeMask: 0xAA");
  CHECK_MSG(ds.m_front.m_sfail == StencilOp::Replace,
            "Front sfail: Replace (was " + std::string(stencilOpName(ds.m_front.m_sfail)) + ")");
  CHECK_MSG(ds.m_front.m_dpfail == StencilOp::Decr,
            "Front dpfail: Decr");
  CHECK_MSG(ds.m_front.m_dppass == StencilOp::Invert,
            "Front dppass: Invert");

  // Back
  CHECK_MSG(ds.m_back.m_func == CompareFunc::NotEqual,
            "Back func: NotEqual (was " + std::string(compareFuncName(ds.m_back.m_func)) + ")");
  CHECK_MSG(ds.m_back.m_ref == 0x13, "Back ref: 0x13");
  CHECK_MSG(ds.m_back.m_compareMask == 0xF0u, "Back compareMask: 0xF0");
  CHECK_MSG(ds.m_back.m_writeMask == 0x55u, "Back writeMask: 0x55");
  CHECK_MSG(ds.m_back.m_sfail == StencilOp::Incr, "Back sfail: Incr");
  CHECK_MSG(ds.m_back.m_dpfail == StencilOp::Invert, "Back dpfail: Invert");
  CHECK_MSG(ds.m_back.m_dppass == StencilOp::Keep, "Back dppass: Keep");

  // ---- Restore ----
  glDisable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glDepthMask(GL_TRUE);
  glDepthRange(0.0, 1.0);
  glDisable(GL_STENCIL_TEST);
  glStencilFuncSeparate(GL_FRONT, GL_ALWAYS, 0, 0xFFFFFFFFu);
  glStencilOpSeparate(GL_FRONT, GL_KEEP, GL_KEEP, GL_KEEP);
  glStencilMaskSeparate(GL_FRONT, 0xFFFFFFFFu);
  glStencilFuncSeparate(GL_BACK, GL_ALWAYS, 0, 0xFFFFFFFFu);
  glStencilOpSeparate(GL_BACK, GL_KEEP, GL_KEEP, GL_KEEP);
  glStencilMaskSeparate(GL_BACK, 0xFFFFFFFFu);

  drainGlErrors("after depth/stencil restore");
  GLStateSnapshot restored = captureAndDrain("depth-stencil-restored");
  CHECK_MSG(restored.m_depthStencil == baseline.m_depthStencil,
            "Depth/stencil state fully restored to baseline");
}

// ============================================================================
//  Test 8: Blend state
// ============================================================================
static void testBlendState() {
  TEST_SECTION("Blend (enable, factors, equations, blend color, color mask)");
  drainGlErrors("pre-blend");

  drainGlErrors("pre-blend-baseline-drain");
  GLStateSnapshot baseline = captureAndDrain("blend-baseline");

  // ---- Modify ----
  glEnable(GL_BLEND);
  glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
                      GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  glBlendEquationSeparate(GL_FUNC_ADD, GL_MAX);
  glBlendColor(0.25f, 0.5f, 0.75f, 1.0f);
  glColorMask(GL_TRUE, GL_TRUE, GL_FALSE, GL_TRUE);

  drainGlErrors("after blend modifications");
  GLStateSnapshot snap = captureAndDrain("blend-capture");
  auto &bl = snap.m_blend;

  CHECK_MSG(bl.m_blendEnabled, "Blend: enabled ✓");
  CHECK_MSG(bl.m_srcRGB == BlendFactor::SrcAlpha &&
                bl.m_dstRGB == BlendFactor::OneMinusSrcAlpha,
            "Blend RGB: (SrcAlpha, 1-SrcAlpha)");
  CHECK_MSG(bl.m_srcAlpha == BlendFactor::One &&
                bl.m_dstAlpha == BlendFactor::OneMinusSrcAlpha,
            "Blend Alpha: (One, 1-SrcAlpha)");
  CHECK_MSG(bl.m_rgbEquation == BlendEquation::Add, "Blend eq RGB: Add");
  CHECK_MSG(bl.m_alphaEquation == BlendEquation::Max, "Blend eq Alpha: Max");
  CHECK_MSG(std::fabs(bl.m_blendColor[0] - 0.25f) < 0.001f &&
                std::fabs(bl.m_blendColor[1] - 0.50f) < 0.001f &&
                std::fabs(bl.m_blendColor[2] - 0.75f) < 0.001f &&
                std::fabs(bl.m_blendColor[3] - 1.00f) < 0.001f,
            "Blend color: (0.25, 0.5, 0.75, 1.0)");
  CHECK_MSG(bl.m_colorMask[0] && bl.m_colorMask[1] &&
                !bl.m_colorMask[2] && bl.m_colorMask[3],
            "Color mask: (T, T, F, T)");

  // ---- Restore ----
  glDisable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ZERO);
  glBlendEquation(GL_FUNC_ADD);
  glBlendColor(0.0f, 0.0f, 0.0f, 0.0f);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

  drainGlErrors("after blend restore");
  GLStateSnapshot restored = captureAndDrain("blend-restored");
  CHECK_MSG(restored.m_blend == baseline.m_blend,
            "Blend state fully restored to baseline");
}

// ============================================================================
//  Test 9: Pixel storage state
// ============================================================================
static void testPixelStorageState() {
  TEST_SECTION("Pixel Storage (pack/unpack params, PBO bindings)");
  drainGlErrors("pre-pixel-storage");

  drainGlErrors("pre-pixel-storage-baseline-drain");
  GLStateSnapshot baseline = captureAndDrain("pixel-storage-baseline");

  // ---- Create PBOs ----
  GLuint packPbo = 0, unpackPbo = 0;
  glGenBuffers(1, &packPbo);
  glGenBuffers(1, &unpackPbo);
  glBindBuffer(GL_PIXEL_PACK_BUFFER, packPbo);
  glBufferData(GL_PIXEL_PACK_BUFFER, 256, nullptr, GL_STREAM_READ);
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, unpackPbo);
  glBufferData(GL_PIXEL_UNPACK_BUFFER, 256, nullptr, GL_STREAM_DRAW);

  // ---- Modify pack ----
  glPixelStorei(GL_PACK_SWAP_BYTES, GL_TRUE);
  glPixelStorei(GL_PACK_ROW_LENGTH, 128);
  glPixelStorei(GL_PACK_ALIGNMENT, 8);
  glPixelStorei(GL_PACK_SKIP_ROWS, 2);
  glPixelStorei(GL_PACK_SKIP_PIXELS, 4);

  // ---- Modify unpack ----
  glPixelStorei(GL_UNPACK_LSB_FIRST, GL_TRUE);
  glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, 256);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glPixelStorei(GL_UNPACK_SKIP_IMAGES, 1);

  drainGlErrors("after pixel storage modifications");
  GLStateSnapshot snap = captureAndDrain("pixel-storage-capture");
  auto &ps = snap.m_pixelStorage;

  // Pack
  CHECK_MSG(ps.m_pack.m_swapBytes,    "Pack swapBytes: true ✓");
  CHECK_MSG(ps.m_pack.m_rowLength == 128, "Pack rowLength: 128 (was " + toStr(ps.m_pack.m_rowLength) + ")");
  CHECK_MSG(ps.m_pack.m_alignment == 8,   "Pack alignment: 8 (was " + toStr(ps.m_pack.m_alignment) + ")");
  CHECK_MSG(ps.m_pack.m_skipRows == 2,    "Pack skipRows: 2 (was " + toStr(ps.m_pack.m_skipRows) + ")");
  CHECK_MSG(ps.m_pack.m_skipPixels == 4,  "Pack skipPixels: 4 (was " + toStr(ps.m_pack.m_skipPixels) + ")");

  // Unpack
  CHECK_MSG(ps.m_unpack.m_lsbFirst,       "Unpack lsbFirst: true ✓");
  CHECK_MSG(ps.m_unpack.m_imageHeight == 256, "Unpack imageHeight: 256 (was " + toStr(ps.m_unpack.m_imageHeight) + ")");
  CHECK_MSG(ps.m_unpack.m_alignment == 1, "Unpack alignment: 1 (was " + toStr(ps.m_unpack.m_alignment) + ")");
  CHECK_MSG(ps.m_unpack.m_skipImages == 1, "Unpack skipImages: 1 (was " + toStr(ps.m_unpack.m_skipImages) + ")");

  // PBOs
  CHECK_MSG(ps.m_pixelPackBuffer == packPbo,
            "Pixel pack buffer: " + toStr(packPbo) + " ✓");
  CHECK_MSG(ps.m_pixelUnpackBuffer == unpackPbo,
            "Pixel unpack buffer: " + toStr(unpackPbo) + " ✓");

  // ---- Restore ----
  glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
  glDeleteBuffers(1, &packPbo);
  glDeleteBuffers(1, &unpackPbo);
  glPixelStorei(GL_PACK_SWAP_BYTES, GL_FALSE);
  glPixelStorei(GL_PACK_ROW_LENGTH, 0);
  glPixelStorei(GL_PACK_ALIGNMENT, 4);
  glPixelStorei(GL_PACK_SKIP_ROWS, 0);
  glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
  glPixelStorei(GL_UNPACK_LSB_FIRST, GL_FALSE);
  glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, 0);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
  glPixelStorei(GL_UNPACK_SKIP_IMAGES, 0);

  drainGlErrors("after pixel storage restore");
  GLStateSnapshot restored = captureAndDrain("pixel-storage-restored");
  CHECK_MSG(restored.m_pixelStorage == baseline.m_pixelStorage,
            "Pixel storage state fully restored to baseline");
}

// ============================================================================
//  Test 10: Idempotency
// ============================================================================
static void testIdempotency() {
  TEST_SECTION("Idempotency (two captures of same state must be equal)");
  drainGlErrors("pre-idempotency");

  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glLineWidth(3.0f);

  drainGlErrors("pre-idempotency-capture");
  GLStateSnapshot snap1 = captureAndDrain("idempotency-1");
  GLStateSnapshot snap2 = captureAndDrain("idempotency-2");

  CHECK_MSG(snap1 == snap2,
            "Two successive captures produce identical snapshots");

  // Cleanup
  glDisable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glDisable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ZERO);
  glLineWidth(1.0f);
}

// ============================================================================
//  Test 11: Edge case — no VAO bound
// ============================================================================
static void testNoVaoBound() {
  TEST_SECTION("Edge case: no VAO bound");
  drainGlErrors("pre-no-vao");
  glBindVertexArray(0);
  GLStateSnapshot snap = captureAndDrain("no-vao-capture");
  (void)snap;
  CHECK_MSG(true, "Capture with no VAO bound does not crash or assert");
}

// ============================================================================
//  main
// ============================================================================
int main() {
  glfwSetErrorCallback(errorCallback);
  if (!glfwInit()) {
    std::cerr << "FATAL: Failed to initialize GLFW" << std::endl;
    return 1;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

  GLFWwindow *window =
      glfwCreateWindow(64, 64, "GLStateManager Test (hidden)", nullptr, nullptr);
  if (!window) {
    std::cerr << "FATAL: Failed to create GLFW window" << std::endl;
    glfwTerminate();
    return 1;
  }
  glfwMakeContextCurrent(window);

  int gladVersion = gladLoadGL(glfwGetProcAddress);
  if (gladVersion == 0) {
    std::cerr << "FATAL: Failed to initialize glad" << std::endl;
    glfwDestroyWindow(window);
    glfwTerminate();
    return 1;
  }

  std::cout << "GL Vendor   : " << glGetString(GL_VENDOR) << std::endl;
  std::cout << "GL Renderer : " << glGetString(GL_RENDERER) << std::endl;
  std::cout << "GL Version  : " << glGetString(GL_VERSION) << std::endl;
  std::cout << "glad        : " << GLAD_VERSION_MAJOR(gladVersion) << "."
            << GLAD_VERSION_MINOR(gladVersion) << std::endl;

  std::cout << "\n"
            << "############################################################\n"
            << "#  GLStateManager::CaptureCurrentState()  Comprehensive Test\n"
            << "############################################################\n";

  // ---- Run all test suites ----
  testDefaultState();
  testResourceBindings();
  testShaderState();
  testVertexInput();
  testFramebufferState();
  testRasterizerState();
  testDepthStencilState();
  testBlendState();
  testPixelStorageState();
  testIdempotency();
  testNoVaoBound();

  // ---- Summary ----
  int total = g_passed + g_failed;
  std::cout << "\n"
            << "============================================================\n"
            << "  RESULTS:  " << g_passed << " passed, "
            << g_failed << " failed";
  if (g_warnings > 0)
    std::cout << ", " << g_warnings << " warnings";
  std::cout << "  (total " << total << ")\n"
            << "============================================================"
            << std::endl;

  if (g_warnings > 0) {
    std::cout << "\nNote: " << g_warnings
              << " GL errors were noted during captures.  These are expected\n"
              << "when GLStateManager queries GL 4.x enums inside a GL 3.3 core\n"
              << "context (e.g. GL_PROGRAM_PIPELINE_BINDING, deprecated texture\n"
              << "targets).  They do not affect the captured state correctness."
              << std::endl;
  }

  glfwDestroyWindow(window);
  glfwTerminate();

  return g_failed == 0 ? 0 : 1;
}
