#include "render/ShaderProgram.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <GLFW/glfw3.h>

namespace voxel {
namespace {
std::string infoLogForObject(const GLhandleARB object) {
    GLint length = 0;
    glGetObjectParameterivARB(object, GL_OBJECT_INFO_LOG_LENGTH_ARB, &length);
    if (length <= 1) {
        return {};
    }

    std::vector<GLcharARB> buffer(static_cast<std::size_t>(length));
    GLsizei written = 0;
    glGetInfoLogARB(object, length, &written, buffer.data());
    return std::string(buffer.begin(), buffer.begin() + written);
}

GLhandleARB compileShader(const GLenum type, const char* source) {
    const GLhandleARB shader = glCreateShaderObjectARB(type);
    if (shader == 0) {
        throw std::runtime_error("Failed to create shader object.");
    }

    glShaderSourceARB(shader, 1, &source, nullptr);
    glCompileShaderARB(shader);

    GLint compiled = 0;
    glGetObjectParameterivARB(shader, GL_OBJECT_COMPILE_STATUS_ARB, &compiled);
    if (compiled == 0) {
        const std::string log = infoLogForObject(shader);
        glDeleteObjectARB(shader);
        throw std::runtime_error("Shader compile failed: " + log);
    }

    return shader;
}
}  // namespace

ShaderProgram::~ShaderProgram() {
    if (program_ != 0) {
        glDeleteObjectARB(program_);
    }
}

void ShaderProgram::initialize() {
    if (program_ != 0) {
        return;
    }

    static const char* kVertexShaderSource = R"glsl(
        varying vec2 vTexCoord;
        varying vec4 vVertexColor;

        void main() {
            gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
            vTexCoord = gl_MultiTexCoord0.xy;
            vVertexColor = gl_Color;
        }
    )glsl";

    static const char* kFragmentShaderSource = R"glsl(
        uniform sampler2D uAlbedo;
        uniform sampler2D uEmissive;
        uniform int uHasEmissive;
        uniform float uOpacity;

        varying vec2 vTexCoord;
        varying vec4 vVertexColor;

        void main() {
            vec4 albedo = texture2D(uAlbedo, vTexCoord);
            vec3 emissive = vec3(0.0);
            if (uHasEmissive != 0) {
                emissive = texture2D(uEmissive, vTexCoord).rgb;
            }

            vec4 color = vec4(albedo.rgb * vVertexColor.rgb + emissive, albedo.a * uOpacity);
            if (color.a <= 0.001) {
                discard;
            }
            gl_FragColor = color;
        }
    )glsl";

    const GLhandleARB vertexShader = compileShader(GL_VERTEX_SHADER_ARB, kVertexShaderSource);
    const GLhandleARB fragmentShader = compileShader(GL_FRAGMENT_SHADER_ARB, kFragmentShaderSource);

    const GLhandleARB program = glCreateProgramObjectARB();
    if (program == 0) {
        glDeleteObjectARB(vertexShader);
        glDeleteObjectARB(fragmentShader);
        throw std::runtime_error("Failed to create shader program.");
    }

    glAttachObjectARB(program, vertexShader);
    glAttachObjectARB(program, fragmentShader);
    glLinkProgramARB(program);

    GLint linked = 0;
    glGetObjectParameterivARB(program, GL_OBJECT_LINK_STATUS_ARB, &linked);
    if (linked == 0) {
        const std::string log = infoLogForObject(program);
        glDeleteObjectARB(vertexShader);
        glDeleteObjectARB(fragmentShader);
        glDeleteObjectARB(program);
        throw std::runtime_error("Shader link failed: " + log);
    }

    glDeleteObjectARB(vertexShader);
    glDeleteObjectARB(fragmentShader);

    program_ = program;
    albedoUniform_ = glGetUniformLocationARB(program, "uAlbedo");
    emissiveUniform_ = glGetUniformLocationARB(program, "uEmissive");
    hasEmissiveUniform_ = glGetUniformLocationARB(program, "uHasEmissive");
    opacityUniform_ = glGetUniformLocationARB(program, "uOpacity");
}

void ShaderProgram::useSurface(
    const TextureResource* albedo,
    const TextureResource* emissive,
    const TextureResource& fallbackBlack,
    const float opacity
) {
    if (program_ == 0 || albedo == nullptr) {
        return;
    }

    glUseProgramObjectARB(program_);

    glActiveTextureARB(GL_TEXTURE0_ARB);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, albedo->glId);
    glUniform1iARB(albedoUniform_, 0);

    glActiveTextureARB(GL_TEXTURE1_ARB);
    glEnable(GL_TEXTURE_2D);
    if (emissive != nullptr) {
        glBindTexture(GL_TEXTURE_2D, emissive->glId);
        glUniform1iARB(hasEmissiveUniform_, 1);
    } else {
        glBindTexture(GL_TEXTURE_2D, fallbackBlack.glId);
        glUniform1iARB(hasEmissiveUniform_, 0);
    }
    glUniform1iARB(emissiveUniform_, 1);
    glUniform1fARB(opacityUniform_, opacity);

    glActiveTextureARB(GL_TEXTURE0_ARB);
}

void ShaderProgram::stop() const {
    glUseProgramObjectARB(0);
    glActiveTextureARB(GL_TEXTURE1_ARB);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);
    glActiveTextureARB(GL_TEXTURE0_ARB);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);
}
}  // namespace voxel
