#include "gfx/OpenGlRenderer.h"

#include <stdexcept>
#include <string>

#include <GL/glew.h> // before any gl.h use in this TU

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
}

void OpenGlRenderer::Initialize(void *windowHandle)
{
#ifdef USE_SDL
    m_window = windowHandle;

    // GL 2.0 request. Don't ask for a forward-compatible context: some old
    // ICDs strip fixed-function entry points (glOrtho, client vertex
    // arrays) from core contexts, and DrawEmulatorFrame uses them.
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);   // 2D UI, no depth buffer
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0); // corners are SDF'd in shader

    SDL_Window *win = static_cast<SDL_Window *>(windowHandle);
    m_context = SDL_GL_CreateContext(win);
    if (!m_context)
        throw std::runtime_error(std::string("OpenGlRenderer: SDL_GL_CreateContext failed: ") + SDL_GetError());
    SDL_GL_MakeCurrent(win, static_cast<SDL_GLContext>(m_context));
    SDL_GL_SetSwapInterval(1); // FIFO - mailbox doesn't exist on GL 2.0

    glewExperimental = GL_TRUE; // old GLEW misses some 2.0 core entries otherwise
    if (glewInit() != GLEW_OK)
        throw std::runtime_error("OpenGlRenderer: glewInit failed");

    SDL_GL_GetDrawableSize(win, reinterpret_cast<int *>(&m_width),
                           reinterpret_cast<int *>(&m_height));
#else
    (void)windowHandle;
    throw std::runtime_error("OpenGlRenderer: no windowing backend compiled in (define USE_SDL)");
#endif
}

void OpenGlRenderer::Shutdown()
{
#ifdef USE_SDL
    // vkDeviceWaitIdle equivalent: GL drains on context teardown.
    if (m_context)
    {
        SDL_GL_DeleteContext(static_cast<SDL_GLContext>(m_context));
        m_context = nullptr;
    }
    m_window = nullptr;
#endif
}

void OpenGlRenderer::OnResize(uint32_t width, uint32_t height)
{
    // VulkanRenderer invalidated its whole per-image framebuffer cache here
    // (swapchain recreation made VkImage handles stale). GL's backbuffer is
    // addressable as FBO 0 regardless of its size - only the viewport
    // changes. (GLint is int32; both are, so the direct store is safe.)
    m_width = width;
    m_height = height;
}

void OpenGlRenderer::BeginFrame(const float clearColor[4])
{
    // The whole VulkanRenderer::RenderEye sequence - reset command buffer,
    // begin, render pass with clear, end, submit, vkQueueWaitIdle - is this:
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0); // backbuffer ("swapchain image")
    glViewport(0, 0, static_cast<GLsizei>(m_width), static_cast<GLsizei>(m_height));
    glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
    glClear(GL_COLOR_BUFFER_BIT);
}

void OpenGlRenderer::EndFrame()
{
#ifdef USE_SDL
    SDL_GL_SwapWindow(static_cast<SDL_Window *>(m_window)); // vkQueuePresentKHR
#endif
}

GLuint OpenGlRenderer::CreateEmulatorTexture(uint32_t maxWidth, uint32_t maxHeight)
{
    GLuint texture = 0;
    glGenTextures(1, &texture);

    glBindTexture(GL_TEXTURE_2D, texture);

    // UiVulkanUtils' staging memcpy was tightly packed; GL's default
    // UNPACK_ALIGNMENT=4 would misread BGRA rows whose width*4 isn't a
    // multiple of 4 (odd widths) - force 1.
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    // NEAREST, matching UiRenderer's pixel-perfect game-screen sampler;
    // CLAMP_TO_EDGE so UV sub-rects never bleed wrapping texels.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Allocation only - nullptr data, filled per frame by
    // UploadEmulatorFrame. GL_RGBA8 is the internal format for both
    // GL_RGBA and GL_BGRA uploads (channel order is a format argument).
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, static_cast<GLsizei>(maxWidth),
                 static_cast<GLsizei>(maxHeight), 0, GL_BGRA, GL_UNSIGNED_BYTE, nullptr);
    CheckGl("glTexImage2D (emulator texture)");

    glBindTexture(GL_TEXTURE_2D, 0);
    return texture;
}

void OpenGlRenderer::UploadEmulatorFrame(GLuint texture, const void *pixels,
                                         uint32_t width, uint32_t height, GLenum uploadFormat)
{
    // The per-frame Vulkan path was: memcpy into the persistent staging
    // buffer -> barrier (SHADER_READ_ONLY -> TRANSFER_DST) ->
    // vkCmdCopyBufferToImage -> barrier back -> submit on the shared
    // command buffer. GL 2.0 has no PBOs and no explicit layouts, so the
    // entire thing is one TexSubImage. The driver does its own staging
    // copy; the texture is sample-ready when this returns - no fence, no
    // vkQueueWaitIdle, usable in the same BeginFrame/EndFrame pair.
    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, static_cast<GLsizei>(width),
                    static_cast<GLsizei>(height), uploadFormat, GL_UNSIGNED_BYTE, pixels);
    CheckGl("glTexSubImage2D (emulator frame)");
    glBindTexture(GL_TEXTURE_2D, 0);
}

void OpenGlRenderer::DrawEmulatorFrame(GLuint texture, float x, float y, float w, float h,
                                       float u0, float v0, float u1, float v1)
{
    // Fixed-function quad - the GL 2.0 equivalent of UiRenderer's
    // DrawUnitQuad into the swapchain image. Ortho matrix maps window
    // pixels directly; y is not flipped (GL texcoord origin matches the
    // top-left convention UiRenderer's NDC math used).
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, static_cast<double>(m_width), static_cast<double>(m_height), 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // DrawImageRegion's alpha fade
    glBindTexture(GL_TEXTURE_2D, texture);

    const GLfloat verts[8] = {x, y, x + w, y, x + w, y + h, x, y + h};
    const GLfloat uvs[8] = {u0, v0, u1, v0, u1, v1, u0, v1};

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glVertexPointer(2, GL_FLOAT, 0, verts);
    glTexCoordPointer(2, GL_FLOAT, 0, uvs);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);

    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);
}

void OpenGlRenderer::GenerateMipmaps(GLuint texture)
{
    // glGenerateMipmap is GL 3.0 core; EXT_framebuffer_object backports it
    // to GL 2.0-era drivers and is present on effectively all of them.
    glBindTexture(GL_TEXTURE_2D, texture);
    glGenerateMipmapEXT(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
}
