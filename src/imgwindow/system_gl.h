#if _MSC_VER                    // compiling via MS Visual Studio
#include <windows.h>            // need to make sure this is read first
#endif

#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl.h>
#elif defined(_WIN32)
#include <GL/gl.h>
// Note: GL/glext.h is not commonly available on Windows
// X-Plane SDK provides necessary OpenGL functionality
#else
#include <GL/gl.h>
#include <GL/glext.h>
#endif

#include "XPLMGraphics.h"
