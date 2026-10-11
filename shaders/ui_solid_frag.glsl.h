#pragma once
// GLSL 1.10 (OpenGL 2.0) source for ui_solid.frag, embedded for runtime
// compilation (GLSL compiles at runtime in GL - this header mirrors
// gfx/generated_shaders/*.h's role for the Vulkan backend).

static const char g_ui_solid_frag_glsl[] =
    "#version 110\n"
    "\n"
    "// Port of ui_solid.frag. Gamma-throughout: the Vulkan original wrote\n"
    "// linear into an sRGB target (hardware encode on store), so\n"
    "// encode(decode(vColor)) was identity and is dropped - vColor is written\n"
    "// as authored. Blending is therefore gamma-space (GL 2.0 backbuffers\n"
    "// aren't sRGB); visually equivalent for UI quads.\n"
    "\n"
    "varying vec2 vUV;\n"
    "\n"
    "uniform vec4 uColor;\n"
    "uniform vec2 uSizePx;\n"
    "uniform float uCornerRadiusPx; // 0 = plain rect\n"
    "uniform float uPixelScale;\n"
    "\n"
    "// Rounded-box SDF (Inigo Quilez) - degenerates to a plain rectangle when\n"
    "// uCornerRadiusPx is 0.\n"
    "float RoundedBoxSDF(vec2 centeredPx, vec2 halfSizePx, float radiusPx) {\n"
    "    vec2 d = abs(centeredPx) - (halfSizePx - vec2(radiusPx));\n"
    "    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0) - radiusPx;\n"
    "}\n"
    "\n"
    "void main() {\n"
    "    float shapeAlpha;\n"
    "    if (uCornerRadiusPx <= 0.0) {\n"
    "        shapeAlpha = 1.0;\n"
    "    } else {\n"
    "        vec2 localPx = vUV * uSizePx;\n"
    "        float dist = RoundedBoxSDF(localPx - uSizePx * 0.5, uSizePx * 0.5, uCornerRadiusPx);\n"
    "        // ~1 physical pixel AA band - dist is in logical units, so the\n"
    "        // band shrinks as uPixelScale grows (see PushConstants' comment).\n"
    "        float aaWidth = 1.0 / uPixelScale;\n"
    "        shapeAlpha = 1.0 - smoothstep(-aaWidth, aaWidth, dist);\n"
    "    }\n"
    "    gl_FragColor = vec4(uColor.rgb, uColor.a * shapeAlpha);\n"
    "}\n"
    "";

static const int g_ui_solid_frag_glsl_size = sizeof(g_ui_solid_frag_glsl);
