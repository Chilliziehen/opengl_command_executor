#ifndef GL_STATE_MANAGER_H
#define GL_STATE_MANAGER_H

#include "GLStateSnapshot.h"

class GLStateManager {
public:
    /// Read the full current GL state into a snapshot (no side effects beyond
    /// restoring the active texture unit it temporarily changes).
    static GLStateSnapshot CaptureCurrentState();

    /// Re-apply a previously captured snapshot to the current context. Restores
    /// bindings (program, VAO, FBOs, textures/samplers/UBOs), fixed-function
    /// state (raster/depth/stencil/blend/pixel-store), and viewport/clear color.
    /// Requires that the referenced GL objects still exist.
    static void RestoreState(const GLStateSnapshot& snapshot);
};

#endif // GL_STATE_MANAGER_H
