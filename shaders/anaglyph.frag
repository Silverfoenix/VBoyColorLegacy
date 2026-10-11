#version 110

// Port of anaglyph.frag (Dubois least-squares matrices - those genuinely
// work on linear light, so this shader KEEPS its explicit
// SrgbToLinear/LinearToSrgb pair; everywhere else the pipeline is
// gamma-throughout). fwidth() -> vUvPerPx * uTexSize. The right-eye UV
// offset gets its own uniform (uRightOffsetU) instead of living in
// patternColors[0].

varying vec2 vUV;
varying vec2 vUvPerPx;

uniform sampler2D uImage;
uniform vec2 uTexSize;
uniform vec4 uUvRect;          // the left eye's part of the texture
uniform float uRightOffsetU;   // the right eye's part, shifted by this
uniform float uGlasses;        // 1 red/cyan, 2 green/magenta, 3 amber/blue

vec3 SrgbToLinear(vec3 c) {
    return mix(c / 12.92, pow((c + 0.055) / 1.055, vec3(2.4)), step(0.04045, c));
}
vec3 LinearToSrgb(vec3 c) {
    return mix(c * 12.92, 1.055 * pow(c, vec3(1.0 / 2.4)) - 0.055, step(0.0031308, c));
}

// A pixel of one eye's picture at t (that eye's texel coordinates):
// sharp pixels with an AA seam, clamped to the eye's own rect.
vec3 SampleEye(vec2 t, vec2 dt, vec2 rectMin, vec2 rectMax) {
    vec2 seam = floor(t + 0.5);
    vec2 p = (t - seam) / dt + seam;
    p = clamp(p, seam - 0.5, seam + 0.5);
    p = clamp(p, rectMin + 0.5, rectMax - 0.5);
    return texture2D(uImage, p / uTexSize).rgb;
}

float Row(vec3 row, vec3 c) { return dot(row, c); }

void main() {
    vec2 t = vUV * uTexSize;
    vec2 dt = max(vUvPerPx * uTexSize, vec2(1e-5));
    vec2 offset = vec2(uRightOffsetU * uTexSize.x, 0.0);
    vec2 rectMin = uUvRect.xy * uTexSize;
    vec2 rectMax = uUvRect.zw * uTexSize;

    vec3 l = SrgbToLinear(SampleEye(t, dt, rectMin, rectMax));
    vec3 r = SrgbToLinear(SampleEye(t + offset, dt, rectMin + offset, rectMax + offset));

    vec3 c;
    float glasses = floor(uGlasses + 0.5);
    if (glasses == 2.0) {
        c.r = Row(vec3(-0.062, -0.158, -0.039), l) + Row(vec3(0.529, 0.705, 0.024), r);
        c.g = Row(vec3(0.284, 0.668, 0.143), l) + Row(vec3(-0.016, -0.015, -0.065), r);
        c.b = Row(vec3(-0.015, -0.027, 0.021), l) + Row(vec3(0.009, 0.075, 0.937), r);
    } else if (glasses == 3.0) {
        c.r = Row(vec3(1.062, -0.205, 0.299), l) + Row(vec3(-0.016, -0.123, -0.017), r);
        c.g = Row(vec3(-0.026, 0.908, 0.068), l) + Row(vec3(0.006, 0.062, -0.017), r);
        c.b = Row(vec3(-0.038, -0.173, 0.022), l) + Row(vec3(0.094, 0.185, 0.911), r);
    } else {
        c.r = Row(vec3(0.4561, 0.500484, 0.176381), l) + Row(vec3(-0.0434706, -0.0879388, -0.00155529), r);
        c.g = Row(vec3(-0.0400822, -0.0378246, -0.0157589), l) + Row(vec3(0.378476, 0.73364, -0.0184503), r);
        c.b = Row(vec3(-0.0152161, -0.0205971, -0.00546856), l) + Row(vec3(-0.0721527, -0.112961, 1.2264), r);
    }
    // Linear result -> encode for the (gamma) backbuffer.
    gl_FragColor = vec4(LinearToSrgb(clamp(c, 0.0, 1.0)), 1.0);
}
