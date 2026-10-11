#pragma once
// GLSL 1.10 (OpenGL 2.0) source for ui_image.frag, embedded for runtime
// compilation (GLSL compiles at runtime in GL - this header mirrors
// gfx/generated_shaders/*.h's role for the Vulkan backend).

static const char g_ui_image_frag_glsl[] =
    "#version 110\n"
    "\n"
    "// Port of ui_image.frag. textureSize() -> uTexSize uniform; fwidth() ->\n"
    "// the vUvPerPx varying (exact constant for this quad - see ui.vert).\n"
    "// Gamma-throughout: sampled texels and the tint both pass through\n"
    "// unchanged (Vulkan's decode-on-read / encode-on-store round-trips were\n"
    "// identity); the tint multiply and alpha fade behave as before.\n"
    "\n"
    "varying vec2 vUV;\n"
    "varying vec2 vUvPerPx;\n"
    "\n"
    "uniform sampler2D uImage;\n"
    "uniform vec2 uTexSize;\n"
    "uniform vec4 uColor;\n"
    "\n"
    "// Anti-aliased pixel-perfect sampling (adapted from shadertoy.com/view/\n"
    "// ltfXWS) - requires LINEAR filtering. vUvPerPx stands in for fwidth.\n"
    "vec4 SamplePixelPerfectAA(sampler2D tex, vec2 uv) {\n"
    "    vec2 uvTexSpace = uv * uTexSize;\n"
    "    vec2 seam = floor(uvTexSpace + 0.5);\n"
    "    uvTexSpace = (uvTexSpace - seam) / vUvPerPx + seam;\n"
    "    uvTexSpace = clamp(uvTexSpace, seam - 0.5, seam + 0.5);\n"
    "    return texture2D(tex, uvTexSpace / uTexSize);\n"
    "}\n"
    "\n"
    "void main() {\n"
    "    gl_FragColor = SamplePixelPerfectAA(uImage, vUV);\n"
    "    // vColor.a: fade a drawn region; vColor.rgb: tint it (white = no-op).\n"
    "    gl_FragColor.a *= uColor.a;\n"
    "    gl_FragColor.rgb *= uColor.rgb;\n"
    "}\n"
    "";

static const int g_ui_image_frag_glsl_size = sizeof(g_ui_image_frag_glsl);
