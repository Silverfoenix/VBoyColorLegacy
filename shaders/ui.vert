#version 110

// OpenGL 2.0 (GLSL 1.10) port of ui.vert. Push constants become plain
// uniforms; the only varyings are vUV and vUvPerPx - everything the
// fragment stage needs per-draw (color, corner radius, size, pattern
// stops, filter mode) arrives as fragment uniforms instead of being
// routed through the vertex stage, since GL 2.0 fragment shaders take
// uniforms directly (the Vulkan versions only passed them down as
// varyings because push constants had to go through the vertex stage to
// reach the fragment stage... actually they didn't, but the layout was
// shared - here we just don't declare the unused ones).
//
// vUvPerPx replaces fwidth(): GLSL 1.10 has no derivative functions, but
// for this axis-aligned unit quad the screen-space UV derivative is a
// constant, computed exactly here: uv span over the quad's physical
// pixel size. Identical value to fwidth(vUV) at every fragment, without
// the per-triangle derivative computation.

attribute vec2 inUnitPos;

uniform vec2 uPosPx;
uniform vec2 uSizePx;
uniform vec4 uUvRect;       // xMin, yMin, xMax, yMax
uniform vec2 uScreenSizePx; // NDC divisor (logical units - see BeginOffscreenFrame)
uniform float uPixelScale;  // physical pixels per logical unit

varying vec2 vUV;
varying vec2 vUvPerPx;

void main() {
    vec2 posPx = uPosPx + inUnitPos * uSizePx;
    vec2 ndc = (posPx / uScreenSizePx) * 2.0 - 1.0;
    gl_Position = vec4(ndc, 0.0, 1.0);
    vUV = mix(uUvRect.xy, uUvRect.zw, inUnitPos);
    vUvPerPx = (uUvRect.zw - uUvRect.xy) / (uSizePx * uPixelScale);
}
