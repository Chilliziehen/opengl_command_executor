cmake_minimum_required(VERSION 3.10)
project(GLStateManager)
set(CMAKE_CXX_STANDARD 20)

add_library(GLStateManager STATIC
    glStateSnapshot/GLStateSnapshot.h
    GLStateManager.h
    glStateSnapshot/ResourceBindingState.h
    glStateSnapshot/BlendState.h
    glStateSnapshot/DepthStencilState.h
    GLStateManager.cpp
)

add_dependencies(GLStateManager thirdparties)

target_include_directories(GLStateManager PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/glStateSnapshot"
    "${CMAKE_SOURCE_DIR}/thirdparty/glfw/include"
    "${GLFW_DEPS_DIR}"
)