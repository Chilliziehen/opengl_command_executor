#include "glStateSnapshot.h"

class GLStateManager{   
public:
    static GLStateSnapshot CaptureCurrentState();
};