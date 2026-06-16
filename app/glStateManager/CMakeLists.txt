add_library(GLStateManager STATIC
    glStateSnapshot/GLStateSnapshot.h
    GLStateManager.h
    glStateSnapshot/ResourceBindingState.h
    glStateSnapshot/BlendState.h
    glStateSnapshot/DepthStencilState.h
    GLStateManager.cpp
)

target_link_libraries(GLStateManager PUBLIC thirdparties)

target_include_directories(GLStateManager PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/glStateSnapshot"
    "${CMAKE_SOURCE_DIR}/thirdparty/glfw/include"
    "${GLFW_DEPS_DIR}"
)