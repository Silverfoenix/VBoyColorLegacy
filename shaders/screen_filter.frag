#version 110

// Port of screen_filter.frag (Looks: 1 Smooth = OmniScale, 2 LED).
// The heavy adaptations, all forced by GLSL 1.10:
//  - texelFetch() -> texture2D at exact texel centers (the LINEAR
//    sampler returns the exact texel there). textureSize() -> uTexSize.
//  - fwidth() -> vUvPerPx varying (see ui.vert) * uTexSize.
//  - No integer types at all: ivec2 -> vec2, and OmniScale's int
//    pattern bitmask becomes a float with exp2()-based bit helpers.
//    P(m, r) is a per-bit compare loop; the diagonal popcount is a
//    15-iteration constant loop.
//  - Gamma-throughout: OmniScale already expected sRGB-encoded input
//    and the target's encode cancelled the shader's final decode, so
//    the stored-value path here is exact. LED's linear-light blend
//    became a gamma-space blend (visually near-identical for a glow).

varying vec2 vUV;
varying vec2 vUvPerPx;

uniform sampler2D uImage;
uniform vec2 uTexSize;
uniform vec4 uUvRect;          // this eye's part of the texture
uniform vec4 uColor;           // .a fades the drawn screen
uniform float uFilterMode;     // 1 Smooth / 2 LED
uniform float uShownPixelSize; // carried in the Vulkan cornerRadiusPx slot

vec2 gRectMin;
vec2 gRectMax;

vec3 Fetch(vec2 p) {
    p = clamp(p, gRectMin, gRectMax);
    return texture2D(uImage, (p + 0.5) / uTexSize).rgb;
}

// --- bit helpers (float stands in for the missing int bitops) ---------

float Bit(float x, float i) {
    return mod(floor(x * exp2(-i)), 2.0);
}

bool P(float pattern, float m, float r) {
    // (pattern & m) == r, one bit at a time.
    for (int i = 0; i < 8; ++i) {
        float fi = float(i);
        float mi = Bit(m, fi);
        if (mi > 0.5 && abs(Bit(pattern, fi) - Bit(r, fi)) > 0.25)
            return false;
        if (mi < 0.5 && Bit(r, fi) > 0.5)
            return false;
    }
    return true;
}

void SetBit(inout float pattern, float i, bool b) {
    if (b)
        pattern += exp2(i);
}

// --- Smooth: OmniScale (SameBoy, Expat/MIT - see the Vulkan original) -

vec3 HqColorspace(vec3 rgb) {
    return vec3(0.250 * rgb.r + 0.250 * rgb.g + 0.250 * rgb.b,
                0.250 * rgb.r - 0.000 * rgb.g - 0.250 * rgb.b,
                -0.125 * rgb.r + 0.250 * rgb.g - 0.125 * rgb.b);
}

bool IsDifferent(vec3 a, vec3 b) {
    vec3 diff = abs(HqColorspace(a - b));
    return diff.x > 0.018 || diff.y > 0.002 || diff.z > 0.005;
}

vec3 OmniScale(vec2 position, float outPixel) {
    vec2 base = floor(position);
    vec2 o = vec2(1.0, 1.0);
    vec2 p = fract(position);
    if (p.x > 0.5) { o.x = -o.x; p.x = 1.0 - p.x; }
    if (p.y > 0.5) { o.y = -o.y; p.y = 1.0 - p.y; }

    vec3 w0 = Fetch(base + vec2(-o.x, -o.y));
    vec3 w1 = Fetch(base + vec2(0.0, -o.y));
    vec3 w2 = Fetch(base + vec2(o.x, -o.y));
    vec3 w3 = Fetch(base + vec2(-o.x, 0.0));
    vec3 w4 = Fetch(base);
    vec3 w5 = Fetch(base + vec2(o.x, 0.0));
    vec3 w6 = Fetch(base + vec2(-o.x, o.y));
    vec3 w7 = Fetch(base + vec2(0.0, o.y));
    vec3 w8 = Fetch(base + vec2(o.x, o.y));

    float pattern = 0.0;
    SetBit(pattern, 0.0, IsDifferent(w0, w4));
    SetBit(pattern, 1.0, IsDifferent(w1, w4));
    SetBit(pattern, 2.0, IsDifferent(w2, w4));
    SetBit(pattern, 3.0, IsDifferent(w3, w4));
    SetBit(pattern, 4.0, IsDifferent(w5, w4));
    SetBit(pattern, 5.0, IsDifferent(w6, w4));
    SetBit(pattern, 6.0, IsDifferent(w7, w4));
    SetBit(pattern, 7.0, IsDifferent(w8, w4));

    if ((P(pattern, 191.0, 55.0) || P(pattern, 219.0, 19.0)) && IsDifferent(w1, w5))
        return mix(w4, w3, 0.5 - p.x);
    if ((P(pattern, 219.0, 73.0) || P(pattern, 239.0, 109.0)) && IsDifferent(w7, w3))
        return mix(w4, w1, 0.5 - p.y);
    if ((P(pattern, 11.0, 11.0) || P(pattern, 254.0, 74.0) || P(pattern, 254.0, 26.0)) && IsDifferent(w3, w1))
        return w4;
    if ((P(pattern, 111.0, 42.0) || P(pattern, 91.0, 10.0) || P(pattern, 191.0, 58.0) ||
         P(pattern, 223.0, 90.0) || P(pattern, 159.0, 138.0) || P(pattern, 207.0, 138.0) ||
         P(pattern, 239.0, 78.0) || P(pattern, 63.0, 14.0) || P(pattern, 251.0, 90.0) ||
         P(pattern, 187.0, 138.0) || P(pattern, 127.0, 90.0) || P(pattern, 175.0, 138.0) ||
         P(pattern, 235.0, 138.0)) && IsDifferent(w3, w1))
        return mix(w4, mix(w4, w0, 0.5 - p.x), 0.5 - p.y);
    if (P(pattern, 11.0, 8.0))
        return mix(mix(w0 * 0.375 + w1 * 0.25 + w4 * 0.375, w4 * 0.5 + w1 * 0.5, p.x * 2.0), w4, p.y * 2.0);
    if (P(pattern, 11.0, 2.0))
        return mix(mix(w0 * 0.375 + w3 * 0.25 + w4 * 0.375, w4 * 0.5 + w3 * 0.5, p.y * 2.0), w4, p.x * 2.0);
    if (P(pattern, 47.0, 47.0)) {
        float dist = length(p - vec2(0.5));
        float pixelSize = outPixel;
        if (dist < 0.5 - pixelSize / 2.0)
            return w4;
        vec3 r;
        if (IsDifferent(w0, w1) || IsDifferent(w0, w3))
            r = mix(w1, w3, p.y - p.x + 0.5);
        else
            r = mix(mix(w1 * 0.375 + w0 * 0.25 + w3 * 0.375, w3, p.y * 2.0), w1, p.x * 2.0);
        if (dist > 0.5 + pixelSize / 2.0)
            return r;
        return mix(w4, r, (dist - 0.5 + pixelSize / 2.0) / pixelSize);
    }
    if (P(pattern, 191.0, 55.0) || P(pattern, 219.0, 19.0)) {
        float dist = p.x - 2.0 * p.y;
        float pixelSize = outPixel * sqrt(5.0);
        if (dist > pixelSize / 2.0)
            return w1;
        vec3 r = mix(w3, w4, p.x + 0.5);
        if (dist < -pixelSize / 2.0)
            return r;
        return mix(r, w1, (dist + pixelSize / 2.0) / pixelSize);
    }
    if (P(pattern, 219.0, 73.0) || P(pattern, 239.0, 109.0)) {
        float dist = p.y - 2.0 * p.x;
        float pixelSize = outPixel * sqrt(5.0);
        if (dist > pixelSize / 2.0)
            return w3;
        vec3 r = mix(w1, w4, p.x + 0.5);
        if (dist < -pixelSize / 2.0)
            return r;
        return mix(r, w3, (dist + pixelSize / 2.0) / pixelSize);
    }
    if (P(pattern, 191.0, 143.0) || P(pattern, 126.0, 14.0)) {
        float dist = p.x + 2.0 * p.y;
        float pixelSize = outPixel * sqrt(5.0);
        if (dist > 1.0 + pixelSize / 2.0)
            return w4;
        vec3 r;
        if (IsDifferent(w0, w1) || IsDifferent(w0, w3))
            r = mix(w1, w3, p.y - p.x + 0.5);
        else
            r = mix(mix(w1 * 0.375 + w0 * 0.25 + w3 * 0.375, w3, p.y * 2.0), w1, p.x * 2.0);
        if (dist < 1.0 - pixelSize / 2.0)
            return r;
        return mix(r, w4, (dist + pixelSize / 2.0 - 1.0) / pixelSize);
    }
    if (P(pattern, 126.0, 42.0) || P(pattern, 239.0, 171.0)) {
        float dist = p.y + 2.0 * p.x;
        float pixelSize = outPixel * sqrt(5.0);
        if (dist > 1.0 + pixelSize / 2.0)
            return w4;
        vec3 r;
        if (IsDifferent(w0, w1) || IsDifferent(w0, w3))
            r = mix(w1, w3, p.y - p.x + 0.5);
        else
            r = mix(mix(w1 * 0.375 + w0 * 0.25 + w3 * 0.375, w3, p.y * 2.0), w1, p.x * 2.0);
        if (dist < 1.0 - pixelSize / 2.0)
            return r;
        return mix(r, w4, (dist + pixelSize / 2.0 - 1.0) / pixelSize);
    }
    if (P(pattern, 27.0, 3.0) || P(pattern, 79.0, 67.0) || P(pattern, 139.0, 131.0) || P(pattern, 107.0, 67.0))
        return mix(w4, w3, 0.5 - p.x);
    if (P(pattern, 75.0, 9.0) || P(pattern, 139.0, 137.0) || P(pattern, 31.0, 25.0) || P(pattern, 59.0, 25.0))
        return mix(w4, w1, 0.5 - p.y);
    if (P(pattern, 251.0, 106.0) || P(pattern, 111.0, 110.0) || P(pattern, 63.0, 62.0) ||
        P(pattern, 251.0, 250.0) || P(pattern, 223.0, 222.0) || P(pattern, 223.0, 30.0))
        return mix(w4, w0, (1.0 - p.x - p.y) / 2.0);
    if (P(pattern, 79.0, 75.0) || P(pattern, 159.0, 27.0) || P(pattern, 47.0, 11.0) ||
        P(pattern, 190.0, 10.0) || P(pattern, 238.0, 10.0) || P(pattern, 126.0, 10.0) ||
        P(pattern, 235.0, 75.0) || P(pattern, 59.0, 27.0)) {
        float dist = p.x + p.y;
        float pixelSize = outPixel;
        if (dist > 0.5 + pixelSize / 2.0)
            return w4;
        vec3 r;
        if (IsDifferent(w0, w1) || IsDifferent(w0, w3))
            r = mix(w1, w3, p.y - p.x + 0.5);
        else
            r = mix(mix(w1 * 0.375 + w0 * 0.25 + w3 * 0.375, w3, p.y * 2.0), w1, p.x * 2.0);
        if (dist < 0.5 - pixelSize / 2.0)
            return r;
        return mix(r, w4, (dist + pixelSize / 2.0 - 0.5) / pixelSize);
    }
    if (P(pattern, 11.0, 1.0))
        return mix(mix(w4, w3, 0.5 - p.x), mix(w1, (w1 + w3) / 2.0, 0.5 - p.x), 0.5 - p.y);
    if (P(pattern, 11.0, 0.0))
        return mix(mix(w4, w3, 0.5 - p.x), mix(w1, w0, 0.5 - p.x), 0.5 - p.y);

    float dist = p.x + p.y;
    float pixelSize = outPixel;
    if (dist > 0.5 + pixelSize / 2.0)
        return w4;

    vec3 x0 = Fetch(base + vec2(-o.x * 2.0, -o.y * 2.0));
    vec3 x1 = Fetch(base + vec2(-o.x, -o.y * 2.0));
    vec3 x2 = Fetch(base + vec2(0.0, -o.y * 2.0));
    vec3 x3 = Fetch(base + vec2(o.x, -o.y * 2.0));
    vec3 x4 = Fetch(base + vec2(-o.x * 2.0, -o.y));
    vec3 x5 = Fetch(base + vec2(-o.x * 2.0, 0.0));
    vec3 x6 = Fetch(base + vec2(-o.x * 2.0, o.y));
    SetBit(pattern, 8.0, IsDifferent(x0, w4));
    SetBit(pattern, 9.0, IsDifferent(x1, w4));
    SetBit(pattern, 10.0, IsDifferent(x2, w4));
    SetBit(pattern, 11.0, IsDifferent(x3, w4));
    SetBit(pattern, 12.0, IsDifferent(x4, w4));
    SetBit(pattern, 13.0, IsDifferent(x5, w4));
    SetBit(pattern, 14.0, IsDifferent(x6, w4));

    float diagonalBias = -7.0;
    for (int i = 0; i < 15; ++i)
        diagonalBias += Bit(pattern, float(i));
    if (diagonalBias <= 0.0) {
        vec3 r = mix(w1, w3, p.y - p.x + 0.5);
        if (dist < 0.5 - pixelSize / 2.0)
            return r;
        return mix(r, w4, (dist + pixelSize / 2.0 - 0.5) / pixelSize);
    }
    return w4;
}

// --- LED: rows of light with dark gaps, blended along the sweep --------

vec3 Led(vec2 position, vec2 outPixel) {
    float x = position.x - 0.5;
    vec2 cell = vec2(floor(x), floor(position.y));
    float fx = smoothstep(0.2, 0.8, x - floor(x));
    vec3 lit = mix(Fetch(cell), Fetch(cell + vec2(1.0, 0.0)), fx);

    float dy = fract(position.y) - 0.5;
    float beam = exp(-dy * dy / (2.0 * 0.26 * 0.26));
    float shown = uShownPixelSize > 0.0 ? uShownPixelSize : 1.0 / outPixel.y;
    float structure = clamp((shown - 2.0) / 2.0, 0.0, 1.0);
    lit *= mix(1.0, beam * 1.3, structure);

    vec3 glow = vec3(0.0);
    float total = 0.0;
    for (int j = -1; j <= 1; ++j)
        for (int i = -1; i <= 1; ++i) {
            vec2 d = vec2(float(i), float(j)) + vec2(0.5) - fract(position);
            float w = exp(-dot(d, d) / (2.0 * 0.9 * 0.9));
            vec3 c = Fetch(floor(position) + vec2(float(i), float(j)));
            glow += c * c * w;
            total += w;
        }
    return lit + glow / total * 0.35;
}

void main() {
    gRectMin = floor(uUvRect.xy * uTexSize + 0.5);
    gRectMax = floor(uUvRect.zw * uTexSize + 0.5) - 1.0;
    float mode = floor(uFilterMode + 0.5);

    // Where this output pixel is in the picture (texels) and how big an
    // output pixel is there - vUvPerPx stands in for fwidth(position).
    vec2 position = vUV * uTexSize;
    vec2 outPixel = vUvPerPx * uTexSize;
    // Rounded so both eyes make the very same decisions (see the Vulkan
    // original's comment) - the same rounding, now exact rather than
    // derivative-derived.
    position = floor((position - gRectMin) * 256.0 + 0.5) / 256.0 + gRectMin;
    outPixel = floor(outPixel * 4096.0 + 0.5) / 4096.0;

    vec3 color = mode == 2.0 ? Led(position, outPixel)
                             : clamp(OmniScale(position, length(outPixel)), 0.0, 1.0);
    gl_FragColor = vec4(color, uColor.a);
}
