#include "gfx/UiOpenGlUtils.h"

#include <cstring>
#include <stdexcept>
#include <string>

// GLEW (or equivalent loader) must come before any gl.h use in this TU.
#include <GL/glew.h>

#ifdef USE_SDL
#include <SDL.h>
#endif

namespace
{
    void CheckGl(const char *what)
    {
        const GLenum err = glGetError();
        if (err != GL_NO_ERROR)
            throw std::runtime_error(std::string("OpenGL call failed: ") + what +
                                     " (glError " + std::to_string(err) + ")");
    }

    bool g_warnedSrgbFallback = false;

    GLint InternalFormat(UiGlFormat format)
    {
        switch (format)
        {
        case UiGlFormat::R8:      return GL_LUMINANCE8;
        case UiGlFormat::Rgba8:   return GL_RGBA8;
        case UiGlFormat::Bgra8:   return GL_RGBA8; // same internal; channel
                                                   // order is a format arg
        case UiGlFormat::Bgra8Srgb:
            if (GLEW_EXT_texture_sRGB)
                return GL_SRGB8_ALPHA8_EXT;
            // Degrade silently-once to plain RGBA8: the pipeline still runs,
            // stored sRGB values just aren't decoded (slightly washed out).
            if (!g_warnedSrgbFallback)
            {
                g_warnedSrgbFallback = true;
                // fprintf(stderr, ...) or your logging hook.
            }
            return GL_RGBA8;
        }
        return GL_RGBA8;
    }

    GLenum UploadFormat(UiGlFormat format)
    {
        switch (format)
        {
        case UiGlFormat::R8:      return GL_LUMINANCE;
        case UiGlFormat::Rgba8:   return GL_RGBA;
        case UiGlFormat::Bgra8:
        case UiGlFormat::Bgra8Srgb:
            return GL_BGRA; // GL_BGRA has been core since GL 1.2
        }
        return GL_RGBA;
    }

    size_t BytesPerPixel(UiGlFormat format)
    {
        return format == UiGlFormat::R8 ? 1 : 4;
    }

    // Shared texture creation. GL's default UNPACK_ALIGNMENT is 4, which
    // misreads tightly-packed rows (the staging memcpy in UiVulkanUtils was
    // tightly packed); force alignment 1 like the Vulkan path assumed.
    GLuint CreateTexture(uint32_t width, uint32_t height, UiGlFormat format, bool linearFilter)
    {
        GLuint tex = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        const GLint filter = linearFilter ? GL_LINEAR : GL_NEAREST;
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glTexImage2D(GL_TEXTURE_2D, 0, InternalFormat(format),
                     static_cast<GLsizei>(width), static_cast<GLsizei>(height), 0,
                     UploadFormat(format), GL_UNSIGNED_BYTE, nullptr);
        CheckGl("glTexImage2D");

        glBindTexture(GL_TEXTURE_2D, 0);
        return tex;
    }

    void UploadPixels(GLuint texture, const void *pixels, uint32_t width, uint32_t height, UiGlFormat format)
    {
        glBindTexture(GL_TEXTURE_2D, texture);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                        static_cast<GLsizei>(width), static_cast<GLsizei>(height),
                        UploadFormat(format), GL_UNSIGNED_BYTE, pixels);
        CheckGl("glTexSubImage2D");
        glBindTexture(GL_TEXTURE_2D, 0);
    }
} // namespace

GLuint UiGlUploadImage(const void *pixels, uint32_t width, uint32_t height,
                       UiGlFormat format, bool linearFilter)
{
    const GLuint tex = CreateTexture(width, height, format, linearFilter);
    UploadPixels(tex, pixels, width, height, format);
    return tex;
}

GLuint UiGlCreateStreamingImage(uint32_t width, uint32_t height, UiGlFormat format)
{
    return CreateTexture(width, height, format, /*linearFilter=*/false);
}

void UiGlUpdateStreamingImage(GLuint texture, const void *pixels, size_t dataSize)
{
    GLint w = 0, h = 0;
    glBindTexture(GL_TEXTURE_2D, texture);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &w);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &h);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Mirrors UiRenderer::UpdateStreamingImage's truncation check: the
    // caller is expected to pass exactly one full surface.
    const size_t expected = static_cast<size_t>(w) * static_cast<size_t>(h) * BytesPerPixel(UiGlFormat::Rgba8);
    if (dataSize < expected)
        return; // same "asserted via truncation, not a hard error" behaviour

    UploadPixels(texture, pixels, static_cast<uint32_t>(w), static_cast<uint32_t>(h), UiGlFormat::Rgba8);
}

GLuint UiGlCreateRenderTexture(uint32_t width, uint32_t height, UiGlFormat format, GLuint &outTexture)
{
    if (!GLEW_EXT_framebuffer_object)
        throw std::runtime_error("UiOpenGlUtils: EXT_framebuffer_object not available");

    outTexture = CreateTexture(width, height, format, /*linearFilter=*/true);

    GLuint fbo = 0;
    glGenFramebuffersEXT(1, &fbo);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
                              GL_TEXTURE_2D, outTexture, 0);
    if (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) != GL_FRAMEBUFFER_COMPLETE_EXT)
    {
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
        glDeleteFramebuffersEXT(1, &fbo);
        glDeleteTextures(1, &outTexture);
        throw std::runtime_error("UiOpenGlUtils: framebuffer incomplete");
    }
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
    return fbo;
}

void UiGlResizeRenderTexture(GLuint framebuffer, GLuint texture,
                             uint32_t width, uint32_t height)
{
    // Keep both names (caller's handles stay valid, mirroring
    // ResizeRenderTexture reusing the descriptor set) - just re-specify the
    // texture storage at the new size and re-attach it.
    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                 static_cast<GLsizei>(width), static_cast<GLsizei>(height), 0,
                 GL_BGRA, GL_UNSIGNED_BYTE, nullptr);
    CheckGl("glTexImage2D (resize)");

    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, framebuffer);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
                              GL_TEXTURE_2D, texture, 0);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
}

void UiGlDestroyRenderTexture(GLuint framebuffer, GLuint texture)
{
    if (framebuffer) glDeleteFramebuffersEXT(1, &framebuffer);
    if (texture)     glDeleteTextures(1, &texture);
}

void *UiGlCreateContext(void *windowHandle)
{
#ifdef USE_SDL
    // 2.0-only context request; on drivers that ignore the hint this still
    // gives a compatibility context (forward-compatible would break FBOs on
    // some old ICDs, so don't ask for it).
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);   // UI is 2D, no depth needed
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0); // rounded corners are SDF'd in-shader

    SDL_GLContext ctx = SDL_GL_CreateContext(static_cast<SDL_Window *>(windowHandle));
    if (!ctx)
        throw std::runtime_error(std::string("UiOpenGlUtils: SDL_GL_CreateContext failed: ") + SDL_GetError());

    SDL_GL_MakeCurrent(static_cast<SDL_Window *>(windowHandle), ctx);
    SDL_GL_SetSwapInterval(1); // mailbox doesn't exist on GL 2.0; vsync on

    glewExperimental = GL_TRUE; // core-entry aliasing on old GLEW
    if (glewInit() != GLEW_OK)
    {
        SDL_GL_DeleteContext(ctx);
        throw std::runtime_error("UiOpenGlUtils: glewInit failed");
    }
    return ctx;
#else
    (void)windowHandle;
    // GLFW path: glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2); etc., or
    // raw wgl: choose a GL 2.0-capable pixel format via
    // wglChoosePixelFormatARB / DescribePixelFormat, then wglCreateContext.
    throw std::runtime_error("UiOpenGlUtils: no windowing backend compiled in (define USE_SDL)");
#endif
}

void UiGlDeleteContext(void *context)
{
#ifdef USE_SDL
    if (context)
        SDL_GL_DeleteContext(static_cast<SDL_GLContext>(context));
#endif
}

void UiGlSwapWindow(void *windowHandle)
{
#ifdef USE_SDL
    SDL_GL_SwapWindow(static_cast<SDL_Window *>(windowHandle));
#else
    (void)windowHandle; // glXSwapBuffers(display, window) / wglSwapLayerBuffers(hdc, WGL_SWAP_MAIN_PLANE)
#endif
}
