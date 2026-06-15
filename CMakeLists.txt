cmake_minimum_required(VERSION 3.15)
project(drawCommandsGL)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/libs")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/libs")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/libs")

include(FetchContent)


FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
    SOURCE_DIR     "${CMAKE_SOURCE_DIR}/thirdparty/glfw" # 强行将源码拉取到指定的 thirdparty 目录下
)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1
    SOURCE_DIR     "${CMAKE_SOURCE_DIR}/thirdparty/glm"
)
set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)

# nlohmann_json — header-only, download single header via CDN (no git needed)
set(JSON_HEADER_DIR  "${CMAKE_SOURCE_DIR}/thirdparty/json")
set(JSON_HEADER_FILE "${JSON_HEADER_DIR}/json.hpp")
if(NOT EXISTS "${JSON_HEADER_FILE}")
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

FetchContent_MakeAvailable(glfw glm)
add_library(glad INTERFACE)
# 顺着路径，让项目能搜到第三方库的头文件
target_include_directories(glad INTERFACE "${CMAKE_SOURCE_DIR}/thirdparty/glfw/deps")

add_library(thirdparties INTERFACE)

target_link_libraries(thirdparties INTERFACE 
    glfw 
    glad
    glm # 某些版本的 GLM 导出名为 glm::glm，如果报错可以尝试切换
    nlohmann_json::nlohmann_json
)

add_library(drawCommandsGL
    src/drawResources/Texture.cpp
    src/framecapture/FrameCapture.cpp
    src/framecapture/CaptureLoader.cpp
)
target_link_libraries(drawCommandsGL PRIVATE thirdparties)
target_include_directories(drawCommandsGL
    PUBLIC  "${CMAKE_SOURCE_DIR}/src"
    PRIVATE "${CMAKE_SOURCE_DIR}/thirdparty/glfw/include"
)

# ---- test executable ----
add_executable(testLoadCapture
    test/testLoadCapture.cpp
)
target_link_libraries(testLoadCapture PRIVATE drawCommandsGL)
target_compile_definitions(testLoadCapture PRIVATE
    PROJECT_ROOT_DIR="${CMAKE_SOURCE_DIR}"
)

