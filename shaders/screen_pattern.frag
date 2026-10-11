#version 110

// Port of screen_pattern.frag. Changes from the Vulkan original:
//  - textureSize() -> uTexSize uniform; fwidth() -> vUvPerPx varying.
//  - pc.patternColors[15] -> uPatternColors[5] (vec3 array).
//  - GLSL 1.10 forbids dynamic uniform-array indexing in fragment
//    shaders, so Stop(seg) became a step()-chained select (4 mix steps
//    for the low stop, 4 for the high stop).
//  - Gamma-throughout: the original decoded the _SRGB texel and
//    re-encoded it to get the stored value back for the luma threshold;
//    here the sampled value IS the stored value. Gradient stops are
//    hand-authored gamma colors and are mixed in gamma space - the
//    Vulkan path's decode->mix->encode reduced to exactly that.

varying vec2 vUV;
varying vec2 vUvPerPx;

uniform sampler2D uImage;
uniform vec2 uTexSize;
uniform vec4 uColor;                       // .a fades the drawn region
uniform vec3 uPatternColors[5];            // darkest -> brightest

vec4 SamplePixelPerfectAA(sampler2D tex, vec2 uv) {
    vec2 uvTexSpace = uv * uTexSize;
    vec2 seam = floor(uvTexSpace + 0.5);
    uvTexSpace = (uvTexSpace - seam) / vUvPerPx + seam;
    uvTexSpace = clamp(uvTexSpace, seam - 0.5, seam + 0.5);
    return texture2D(tex, uvTexSpace / uTexSize);
}

void main() {
    vec4 texColor = SamplePixelPerfectAA(uImage, vUV);
    // Thresholds calibrated against the core's raw 0-255 output value,
    // which is the stored (gamma) value - exactly what we sampled.
    float luma = dot(texColor.rgb, vec3(0.2126, 0.7152, 0.0722));

    // 5 stops -> 4 segments over luma 0..1. Select the bracketing stops
    // without dynamic indexing: step() chains, then mix by the fraction.
    float scaled = clamp(luma, 0.0, 1.0) * 4.0;
    vec3 c0 = uPatternColors[0];
    vec3 c1 = uPatternColors[1];
    vec3 c2 = uPatternColors[2];
    vec3 c3 = uPatternColors[3];
    vec3 c4 = uPatternColors[4];
    vec3 lo = mix(mix(mix(c0, c1, step(1.0, scaled)),
                      c2, step(2.0, scaled)),
                  c3, step(3.0, scaled));
    vec3 hi = mix(mix(mix(c1, c2, step(2.0, scaled)),
                      c3, step(3.0, scaled)),
                  c4, step(4.0, scaled));
    // f = scaled - floor(scaled), but pinned to 1 at the top stop
    // (scaled == 4 must give c4, and fract(4.0) == 0).
    float f = scaled - min(floor(scaled), 3.0);
    vec3 patternColor = mix(lo, hi, clamp(f, 0.0, 1.0));

    gl_FragColor = vec4(patternColor, texColor.a * uColor.a);
}
