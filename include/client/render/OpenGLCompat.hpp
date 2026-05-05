#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>

#include <GLFW/glfw3.h>
#if defined(__APPLE__)
#include <OpenGL/glext.h>
#elif !defined(_WIN32)
#include <GL/glext.h>
#endif

#if defined(_WIN32)
using GLcharARB = char;
using GLhandleARB = unsigned int;
#endif

#ifndef GL_VERSION_1_5
using GLsizeiptr = std::ptrdiff_t;
#endif

#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER 0x8892
#endif

#ifndef GL_STATIC_DRAW
#define GL_STATIC_DRAW 0x88E4
#endif

#ifndef GL_TEXTURE0_ARB
#define GL_TEXTURE0_ARB 0x84C0
#endif

#ifndef GL_TEXTURE1_ARB
#define GL_TEXTURE1_ARB 0x84C1
#endif

#ifndef GL_VERTEX_SHADER_ARB
#define GL_VERTEX_SHADER_ARB 0x8B31
#endif

#ifndef GL_FRAGMENT_SHADER_ARB
#define GL_FRAGMENT_SHADER_ARB 0x8B30
#endif

#ifndef GL_OBJECT_COMPILE_STATUS_ARB
#define GL_OBJECT_COMPILE_STATUS_ARB 0x8B81
#endif

#ifndef GL_OBJECT_LINK_STATUS_ARB
#define GL_OBJECT_LINK_STATUS_ARB 0x8B82
#endif

#ifndef GL_OBJECT_INFO_LOG_LENGTH_ARB
#define GL_OBJECT_INFO_LOG_LENGTH_ARB 0x8B84
#endif

namespace voxel {
namespace gl_compat {
template <typename Proc>
Proc loadProc(const char* name) {
    auto* proc = glfwGetProcAddress(name);
    if (proc == nullptr) {
        throw std::runtime_error(std::string("Required OpenGL function unavailable: ") + name);
    }
    return reinterpret_cast<Proc>(proc);
}
}  // namespace gl_compat

inline void glGenBuffers(GLsizei n, GLuint* buffers) {
    using Proc = void(APIENTRY*)(GLsizei, GLuint*);
    static Proc proc = gl_compat::loadProc<Proc>("glGenBuffers");
    proc(n, buffers);
}

inline void glBindBuffer(GLenum target, GLuint buffer) {
    using Proc = void(APIENTRY*)(GLenum, GLuint);
    static Proc proc = gl_compat::loadProc<Proc>("glBindBuffer");
    proc(target, buffer);
}

inline void glBufferData(GLenum target, GLsizeiptr size, const void* data, GLenum usage) {
    using Proc = void(APIENTRY*)(GLenum, GLsizeiptr, const void*, GLenum);
    static Proc proc = gl_compat::loadProc<Proc>("glBufferData");
    proc(target, size, data, usage);
}

inline void glDeleteBuffers(GLsizei n, const GLuint* buffers) {
    using Proc = void(APIENTRY*)(GLsizei, const GLuint*);
    static Proc proc = gl_compat::loadProc<Proc>("glDeleteBuffers");
    proc(n, buffers);
}

inline void glGetObjectParameterivARB(GLhandleARB object, GLenum pname, GLint* params) {
    using Proc = void(APIENTRY*)(GLhandleARB, GLenum, GLint*);
    static Proc proc = gl_compat::loadProc<Proc>("glGetObjectParameterivARB");
    proc(object, pname, params);
}

inline void glGetInfoLogARB(GLhandleARB object, GLsizei maxLength, GLsizei* length, GLcharARB* infoLog) {
    using Proc = void(APIENTRY*)(GLhandleARB, GLsizei, GLsizei*, GLcharARB*);
    static Proc proc = gl_compat::loadProc<Proc>("glGetInfoLogARB");
    proc(object, maxLength, length, infoLog);
}

inline GLhandleARB glCreateShaderObjectARB(GLenum shaderType) {
    using Proc = GLhandleARB(APIENTRY*)(GLenum);
    static Proc proc = gl_compat::loadProc<Proc>("glCreateShaderObjectARB");
    return proc(shaderType);
}

inline void glShaderSourceARB(GLhandleARB shader, GLsizei count, const GLcharARB** string, const GLint* length) {
    using Proc = void(APIENTRY*)(GLhandleARB, GLsizei, const GLcharARB**, const GLint*);
    static Proc proc = gl_compat::loadProc<Proc>("glShaderSourceARB");
    proc(shader, count, string, length);
}

inline void glCompileShaderARB(GLhandleARB shader) {
    using Proc = void(APIENTRY*)(GLhandleARB);
    static Proc proc = gl_compat::loadProc<Proc>("glCompileShaderARB");
    proc(shader);
}

inline void glDeleteObjectARB(GLhandleARB object) {
    using Proc = void(APIENTRY*)(GLhandleARB);
    static Proc proc = gl_compat::loadProc<Proc>("glDeleteObjectARB");
    proc(object);
}

inline GLhandleARB glCreateProgramObjectARB() {
    using Proc = GLhandleARB(APIENTRY*)();
    static Proc proc = gl_compat::loadProc<Proc>("glCreateProgramObjectARB");
    return proc();
}

inline void glAttachObjectARB(GLhandleARB containerObj, GLhandleARB obj) {
    using Proc = void(APIENTRY*)(GLhandleARB, GLhandleARB);
    static Proc proc = gl_compat::loadProc<Proc>("glAttachObjectARB");
    proc(containerObj, obj);
}

inline void glLinkProgramARB(GLhandleARB programObj) {
    using Proc = void(APIENTRY*)(GLhandleARB);
    static Proc proc = gl_compat::loadProc<Proc>("glLinkProgramARB");
    proc(programObj);
}

inline GLint glGetUniformLocationARB(GLhandleARB programObj, const GLcharARB* name) {
    using Proc = GLint(APIENTRY*)(GLhandleARB, const GLcharARB*);
    static Proc proc = gl_compat::loadProc<Proc>("glGetUniformLocationARB");
    return proc(programObj, name);
}

inline void glUseProgramObjectARB(GLhandleARB programObj) {
    using Proc = void(APIENTRY*)(GLhandleARB);
    static Proc proc = gl_compat::loadProc<Proc>("glUseProgramObjectARB");
    proc(programObj);
}

inline void glActiveTextureARB(GLenum texture) {
    using Proc = void(APIENTRY*)(GLenum);
    static Proc proc = gl_compat::loadProc<Proc>("glActiveTextureARB");
    proc(texture);
}

inline void glUniform1iARB(GLint location, GLint v0) {
    using Proc = void(APIENTRY*)(GLint, GLint);
    static Proc proc = gl_compat::loadProc<Proc>("glUniform1iARB");
    proc(location, v0);
}

inline void glUniform1fARB(GLint location, GLfloat v0) {
    using Proc = void(APIENTRY*)(GLint, GLfloat);
    static Proc proc = gl_compat::loadProc<Proc>("glUniform1fARB");
    proc(location, v0);
}
}  // namespace voxel
