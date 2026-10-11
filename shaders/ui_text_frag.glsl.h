#pragma once
// GLSL 1.10 (OpenGL 2.0) source for ui_text.frag, embedded for runtime
// compilation (GLSL compiles at runtime in GL - this header mirrors
// gfx/generated_shaders/*.h's role for the Vulkan backend).

static const char g_ui_text_frag_glsl[] =
    "#version 110\n"
    "\n"
    "// Port of ui_text.frag. The glyph atlas is uploaded as GL_LUMINANCE, so\n"
    "// .r carries the coverage value directly. Gamma-throughout - see\n"
    "// ui_solid.frag's header comment.\n"
    "\n"
    "varying vec2 vUV;\n"
    "\n"
    "uniform sampler2D uAtlas;\n"
    "uniform vec4 uColor;\n"
    "\n"
    "void main() {\n"
    "    float a = texture2D(uAtlas, vUV).r;\n"
    "    gl_FragColor = vec4(uColor.rgb, uColor.a * a);\n"
    "}\n"
    "";

static const int g_ui_text_frag_glsl_size = sizeof(g_ui_text_frag_glsl);
