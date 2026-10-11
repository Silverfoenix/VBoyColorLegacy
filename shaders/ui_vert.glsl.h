#pragma once
// GLSL 1.10 (OpenGL 2.0) source for ui.vert, embedded for runtime
// compilation (GLSL compiles at runtime in GL - this header mirrors
// gfx/generated_shaders/*.h's role for the Vulkan backend).

static const char g_ui_vert_glsl[] =
    "#version 110\n"
    "\n"
    "// OpenGL 2.0 (GLSL 1.10) port of ui.vert. Push constants become plain\n"
    "// uniforms; the only varyings are vUV and vUvPerPx - everything the\n"
    "// fragment stage needs per-draw (color, corner radius, size, pattern\n"
    "// stops, filter mode) arrives as fragment uniforms instead of being\n"
    "// routed through the vertex stage, since GL 2.0 fragment shaders take\n"
    "// uniforms directly (the Vulkan versions only passed them down as\n"
    "// varyings because push constants had to go through the vertex stage to\n"
    "// reach the fragment stage... actually they didn't, but the layout was\n"
    "// shared - here we just don't declare the unused ones).\n"
    "//\n"
    "// vUvPerPx replaces fwidth(): GLSL 1.10 has no derivative functions, but\n"
    "// for this axis-aligned unit quad the screen-space UV derivative is a\n"
    "// constant, computed exactly here: uv span over the quad's physical\n"
    "// pixel size. Identical value to fwidth(vUV) at every fragment, without\n"
    "// the per-triangle derivative computation.\n"
    "\n"
    "attribute vec2 inUnitPos;\n"
    "\n"
    "uniform vec2 uPosPx;\n"
    "uniform vec2 uSizePx;\n"
    "uniform vec4 uUvRect;       // xMin, yMin, xMax, yMax\n"
    "uniform vec2 uScreenSizePx; // NDC divisor (logical units - see BeginOffscreenFrame)\n"
    "uniform float uPixelScale;  // physical pixels per logical unit\n"
    "\n"
    "varying vec2 vUV;\n"
    "varying vec2 vUvPerPx;\n"
    "\n"
    "void main() {\n"
    "    vec2 posPx = uPosPx + inUnitPos * uSizePx;\n"
    "    vec2 ndc = (posPx / uScreenSizePx) * 2.0 - 1.0;\n"
    "    gl_Position = vec4(ndc, 0.0, 1.0);\n"
    "    vUV = mix(uUvRect.xy, uUvRect.zw, inUnitPos);\n"
    "    vUvPerPx = (uUvRect.zw - uUvRect.xy) / (uSizePx * uPixelScale);\n"
    "}\n"
    "";

static const int g_ui_vert_glsl_size = sizeof(g_ui_vert_glsl);
