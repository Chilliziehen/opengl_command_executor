#include "GLStateSnapshot.h"

class GLStateManager{   
public:
    static GLStateSnapshot CaptureCurrentState();
};