#pragma once

#include <cstddef>
#include <cstdint>
#include <GL/gl.h>

// OpenGL 2.0 replacement for VulkanRenderer - windowed/desktop path only.
//
// VulkanRenderer owned two creation paths; this class ports the standalone
// windowed one (CreateInstanceStandalone + CreateDeviceForSurface). The
// OpenXR delegated-creation path (CreateDevice via xrCreateVulkanInstanceKHR
// /xrCreateVulkanDeviceKHR, GetGraphicsBinding, RenderEye on projection-layer
// swapchain images) has NO OpenGL 2.0 equivalent: XR_KHR_opengl_enable
// requires a GL 3.3+ context. On this legacy backend the window backbuffer
// is the only render target, which deletes most of VulkanRenderer:
//  - instance/device/physical-device selection -> one SDL GL context
//  - queue family / command pool / shared command buffer -> nothing (GL
//    orders work implicitly; UiRenderer's GL equivalent never needed the
//    "borrowed command buffer" arrangement at all)
//  - render pass + per-VkImage framebuffer cache -> FBO 0, always
//  - vkQueueWaitIdle after every submission -> nothing
//  - InvalidateRenderTargets on swapchain recreation -> glViewport only
//
// Emulator-frame pipeline (the reason this file exists): the Virtual Boy's
// framebuffer arrives as a plain CPU pixel array every emulated frame.
// Vulkan uploaded it through CreateStreamingImage's persistent staging
// buffer + vkCmdCopyBufferToImage; here it is a glTexSubImage2D into a
// texture allocated once at max frame size (see CreateEmulatorTexture).
class OpenGlRenderer
{
public:
    // Creates a GL 2.0 context on the window and loads extensions (GLEW).
    // windowHandle is your SDL_Window* (adjust the casts in the .cpp for
    // GLFW/wgl/glx). Replaces CreateInstanceStandalone +
    // CreateDeviceForSurface + FinishDeviceSetup in one step.
    void Initialize(void *windowHandle);
    void Shutdown();

    uint32_t GetWidth() const { return m_width; }
    uint32_t GetHeight() const { return m_height; }

    // Window resized: in Vulkan this meant swapchain recreation plus
    // InvalidateRenderTargets; here it is just a viewport update (the
    // backbuffer FBO is recreated by the driver behind SDL_GL_SwapWindow).
    void OnResize(uint32_t width, uint32_t height);

    // Binds the backbuffer (FBO 0), sets viewport, clears to clearColor.
    // Absorbs VulkanRenderer::RenderEye's whole job (its only remaining
    // purpose was clearing the target black before UiRenderer drew content
    // on top) - the render pass / framebuffer lookup / command buffer /
    // queue submit / vkQueueWaitIdle sequence collapses into glClear.
    void BeginFrame(const float clearColor[4]);
    // Presents: SDL_GL_SwapWindow (vsync was enabled at Initialize).
    void EndFrame();

    // --- Emulator framebuffer pipeline -----------------------------------
    // One-time allocation sized for the largest frame the emulator can
    // output (replaces CreateStreamingImage). The texture is created
    // empty; UploadEmulatorFrame fills it. NEAREST filtering, matching
    // UiRenderer's pixel-perfect game-screen sampler.
    GLuint CreateEmulatorTexture(uint32_t maxWidth, uint32_t maxHeight);

    // Pushes one frame of CPU pixels into the texture (replaces
    // UpdateStreamingImage's staging memcpy + copy command + layout
    // barrier). width/height are THIS frame's size and must be <= the
    // max size the texture was created with; draw a UV-cropped sub-rect
    // (DrawEmulatorFrame's uv params) for frames smaller than the max.
    // pixels are tightly packed BGRA8 (GL's fast path, matching
    // VulkanRenderer::SelectSwapchainFormat's B8G8R8A8 preference);
    // pass a different uploadFormat if the emulator core hands you RGBA.
    void UploadEmulatorFrame(GLuint texture, const void *pixels,
                             uint32_t width, uint32_t height,
                             GLenum uploadFormat = GL_BGRA);

    // Draws the uploaded frame as a textured bounding box at (x, y, w, h)
    // in window pixels - the fixed-function equivalent of UiRenderer's
    // DrawImage into the swapchain. Pixel-perfect: integer-scale w/h
    // before calling, NEAREST does the rest. u0..v1 allow drawing a
    // sub-rect of a max-sized texture for smaller frames (DrawImageRegion).
    void DrawEmulatorFrame(GLuint texture, float x, float y, float w, float h,
                           float u0 = 0.0f, float v0 = 0.0f, float u1 = 1.0f, float v1 = 1.0f);

    // Equivalent of VulkanRenderer::GenerateMipmaps (kept for the same
    // anti-shimmer reason: any minified resampling of the layer aliases
    // without a mip chain). Uses glGenerateMipmapEXT from
    // EXT_framebuffer_object, universal on GL 2.0 hardware.
    void GenerateMipmaps(GLuint texture);

private:
    void *m_window{nullptr};     // SDL_Window*, not owned
    void *m_context{nullptr};    // SDL_GLContext
    uint32_t m_width{0};
    uint32_t m_height{0};
};
