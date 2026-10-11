#pragma once

// Legacy OpenGL 2.0 replacement for UiVulkanUtils.h.
//
// Vulkan concepts that simply vanish under GL 2.0:
//  - UiFindMemoryType / VkDeviceMemory: GL drivers pick and own memory.
//  - Staging buffers: glTexImage2D *is* the staged upload.
//  - Layout transitions (UNDEFINED -> TRANSFER_DST -> SHADER_READ_ONLY):
//    a texture is sample-ready the moment glTexImage2D returns.
//  - One-time command buffers + vkQueueWaitIdle: uploads are ordered
//    implicitly; a texture uploaded this frame is samplable this frame.
//
// Window context / swapchain (previously VulkanRenderer's job):
//  - The VkSurfaceKHR/VkSwapchainKHR pair becomes a GL context created on
//    the window + the default framebuffer (0). BeginFrame() equivalent:
//    glBindFramebuffer(GL_FRAMEBUFFER, 0) + glViewport/glScissor.
//    EndFrame() equivalent: UiGlSwapWindow() below (one SwapBuffers call,
//    vsync via SDL_GL_SetSwapInterval).
//  - VulkanRenderer's per-swapchain-image VkFramebuffer cache collapses
//    to FBO 0 - there is only one back buffer binding point.
//
// Requires an extension loader (GLEW, or SDL_GL_GetProcAddress) available
// before this header is used in a TU, for EXT_framebuffer_object and
// optionally EXT_texture_sRGB.

#include <cstddef>
#include <cstdint>
#include <GL/gl.h>

// Texture formats matching the VkFormats UiRenderer passes today.
enum class UiGlFormat
{
    R8,        // VK_FORMAT_R8_UNORM        -> GL_LUMINANCE  (glyph atlas)
    Rgba8,     // VK_FORMAT_R8G8B8A8_UNORM  -> GL_RGBA
    Bgra8,     // VK_FORMAT_B8G8R8A8_UNORM  -> GL_BGRA (usually fastest)
    Bgra8Srgb  // BGRA stored sRGB-encoded; uses EXT_texture_sRGB when
               // present, otherwise degrades to Bgra8 with a one-time
               // warning (colors slightly washed out - no decode happens).
};

// Direct upload of CPU pixels to a new GL texture - the GL 2.0 equivalent
// of UiUploadImage(). Sampler state lives on the texture in GL (Vulkan kept
// it in the descriptor set), so it is set here: linearFilter=false gives
// the NEAREST sampler UiRenderer uses for pixel-perfect game screens.
// Ownership: caller glDeleteTextures() at teardown (no outMemory equivalent).
GLuint UiGlUploadImage(const void *pixels, uint32_t width, uint32_t height,
                       UiGlFormat format, bool linearFilter = true);

// Streaming variant for per-frame content (emulator video at ~50Hz) -
// equivalent of UiRenderer::CreateStreamingImage()/UpdateStreamingImage().
// GL 2.0 has no PBOs, so the persistent staging buffer collapses to a
// plain glTexSubImage2D; the driver still avoids the allocation churn that
// made UiUploadImage's destroy-and-recreate too slow at frame rate.
GLuint UiGlCreateStreamingImage(uint32_t width, uint32_t height, UiGlFormat format);

// dataSize must be width*height*bytesPerPixel of the size the texture was
// created with (mirrors UiRenderer::UpdateStreamingImage: full-surface
// upload; draw a UV-cropped sub-rect via DrawImageRegion for smaller frames).
void UiGlUpdateStreamingImage(GLuint texture, const void *pixels, size_t dataSize);

// Render-to-texture equivalent of UiRenderer::CreateRenderTexture():
// needs EXT_framebuffer_object (present on effectively all GL 2.0 hardware).
// Returns the FBO; *outTexture receives the attached color texture.
// Render into it with glBindFramebuffer(GL_FRAMEBUFFER_EXT, fbo); sample
// the texture with the normal image pipeline. The handle stays valid across
// UiGlResizeRenderTexture (mirrors ResizeRenderTexture reusing the
// descriptor set): the texture is re-specified and re-attached in place.
GLuint UiGlCreateRenderTexture(uint32_t width, uint32_t height,
                               UiGlFormat format, GLuint &outTexture);
void UiGlResizeRenderTexture(GLuint framebuffer, GLuint texture,
                             uint32_t width, uint32_t height);
void UiGlDestroyRenderTexture(GLuint framebuffer, GLuint texture);

// Window "swapchain" equivalent: call after EndFrame. In Vulkan this was
// ending the render pass + presenting a swapchain image; in GL 2.0 the
// swapchain is the default framebuffer plus one platform swap call.
// windowHandle is your SDL_Window* (adjust the cast in the .cpp if the
// engine uses GLFW or raw wgl/glx instead).
void UiGlSwapWindow(void *windowHandle);

// One-time init: creates the GL context on the window with a GL 2.0
// pixel format and loads extensions. Replaces VulkanRenderer's
// instance/device/swapchain creation for the window path. Call once before
// UiRenderer's GL equivalent Initialize(); returns the context for the
// caller to keep alive and delete at teardown.
void *UiGlCreateContext(void *windowHandle);   // returns SDL_GLContext (or your ctx type)
void UiGlDeleteContext(void *context);
