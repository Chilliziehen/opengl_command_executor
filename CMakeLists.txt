cmake_minimum_required(VERSION 3.15)
project(drawCommandsGL)

configure_file(${CMAKE_SOURCE_DIR}/config/config.in ${CMAKE_SOURCE_DIR}/config.h)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/out")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/out")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/out")

include(FetchContent)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
    SOURCE_DIR "${CMAKE_SOURCE_DIR}/thirdparty/glfw" # 强行将源码拉取到指定的 thirdparty 目录下
)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
    SOURCE_DIR "${CMAKE_SOURCE_DIR}/thirdparty/glm"
)
set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)

# nlohmann_json — header-only, download single header via CDN (no git needed)
set(JSON_HEADER_DIR "${CMAKE_SOURCE_DIR}/thirdparty/json")
set(JSON_HEADER_FILE "${JSON_HEADER_DIR}/nlohmann/json.hpp")

if(NOT EXISTS "${JSON_HEADER_FILE}")
    file(MAKE_DIRECTORY "${JSON_HEADER_DIR}/nlohmann")
    file(DOWNLOAD
        "https://cdn.jsdelivr.net/gh/nlohmann/json@v3.11.3/single_include/nlohmann/json.hpp"
        "${JSON_HEADER_FILE}"
        INACTIVITY_TIMEOUT 30
        STATUS downloadStatus
    )
    list(GET downloadStatus 0 statusCode)

    if(NOT statusCode EQUAL 0)
        message(WARNING "Cannot download json.hpp — place it manually at ${JSON_HEADER_FILE}")
    endif()
endif()

add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE "${JSON_HEADER_DIR}")
add_library(nlohmann_json::nlohmann_json ALIAS nlohmann_json)

# Dear ImGui — GUI for the per-command replayer (no CMakeLists of its own, so we
# just fetch the sources and compile them into a static lib below).
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.90.9
    SOURCE_DIR "${CMAKE_SOURCE_DIR}/thirdparty/imgui"
)

FetchContent_MakeAvailable(glfw glm)
FetchContent_MakeAvailable(imgui)

# GLFW deps 目录（包含 glad 等）——必须在引用 glad 之前定义
set(GLFW_DEPS_DIR "${CMAKE_SOURCE_DIR}/thirdparty/glfw/deps")

add_library(glad INTERFACE)

# 顺着路径，让项目能搜到第三方库的头文件
target_include_directories(glad INTERFACE "${GLFW_DEPS_DIR}")

add_library(thirdparties INTERFACE)

target_link_libraries(thirdparties INTERFACE
    glfw
    glad
    glm # 某些版本的 GLM 导出名为 glm::glm，如果报错可以尝试切换
    nlohmann_json::nlohmann_json
)

# glStateManager (added via add_subdirectory below) references GLFW_DEPS_DIR for
# the glad headers; define it here so it's set before that subdirectory.
set(GLFW_DEPS_DIR "${CMAKE_SOURCE_DIR}/thirdparty/glfw/deps")

# ---- Dear ImGui static lib (core + GLFW/OpenGL3 backends) ----
add_library(imgui STATIC
    "${imgui_SOURCE_DIR}/imgui.cpp"
    "${imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_SOURCE_DIR}/imgui_tables.cpp"
    "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp"
)
target_include_directories(imgui PUBLIC
    "${imgui_SOURCE_DIR}"
    "${imgui_SOURCE_DIR}/backends"
    "${CMAKE_SOURCE_DIR}/thirdparty/glfw/include"
)
target_link_libraries(imgui PUBLIC glfw)

add_library(drawCommandsGLlib
    src/core/GladImplementation.cpp
    src/drawResources/Texture.cpp
    src/drawResources/Command.cpp
    src/framecapture/FrameCapture.cpp
    src/framecapture/CaptureLoader.cpp
    src/resourceManagement/ResourceAllocator.cpp
    src/resourceManagement/ResourceManager.cpp
    src/shaderTranslation/ShaderInterpreter.cpp
    src/replayer/ReplayEngine.cpp
)
target_link_libraries(drawCommandsGLlib PRIVATE thirdparties)
target_include_directories(drawCommandsGLlib
    PUBLIC "${CMAKE_SOURCE_DIR}/src"
    PRIVATE "${CMAKE_SOURCE_DIR}/thirdparty/glfw/include"
)

# ---- test executables ----
add_executable(testLoadCapture
    test/testLoadCapture.cpp
)
target_link_libraries(testLoadCapture PRIVATE drawCommandsGLlib)
target_compile_definitions(testLoadCapture PRIVATE
    PROJECT_ROOT_DIR="${CMAKE_SOURCE_DIR}"
)

add_executable(testReplayCapture
    test/testReplayCapture.cpp
)
target_link_libraries(testReplayCapture PRIVATE drawCommandsGLlib thirdparties)
target_compile_definitions(testReplayCapture PRIVATE
    PROJECT_ROOT_DIR="${CMAKE_SOURCE_DIR}"
)

add_executable(testShaderTranslate
    test/testShaderTranslate.cpp
)
target_link_libraries(testShaderTranslate PRIVATE drawCommandsGLlib)
target_compile_definitions(testShaderTranslate PRIVATE
    PROJECT_ROOT_DIR="${CMAKE_SOURCE_DIR}"
)

add_subdirectory(app/glStateManager)

# ---- GLStateManager capture test ----
add_executable(testGLStateManager
    test/testGLStateManager.cpp
)
target_link_libraries(testGLStateManager PRIVATE drawCommandsGLlib thirdparties GLStateManager)
target_include_directories(testGLStateManager PRIVATE
    "${CMAKE_SOURCE_DIR}/app/glStateManager"
    "${CMAKE_SOURCE_DIR}/app/glStateManager/glStateSnapshot"
)

# ---- GUI per-command replayer ----
add_executable(guiReplayer
    app/guiReplayer.cpp
)
target_link_libraries(guiReplayer PRIVATE drawCommandsGLlib thirdparties imgui GLStateManager)
target_include_directories(guiReplayer PRIVATE
    "${CMAKE_SOURCE_DIR}/app/glStateManager"
)
target_compile_definitions(guiReplayer PRIVATE
    PROJECT_ROOT_DIR="${CMAKE_SOURCE_DIR}"
)
