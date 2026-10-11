#version 110

// Port of ui_solid.frag. Gamma-throughout: the Vulkan original wrote
// linear into an sRGB target (hardware encode on store), so
// encode(decode(vColor)) was identity and is dropped - vColor is written
// as authored. Blending is therefore gamma-space (GL 2.0 backbuffers
// aren't sRGB); visually equivalent for UI quads.

varying vec2 vUV;

uniform vec4 uColor;
uniform vec2 uSizePx;
uniform float uCornerRadiusPx; // 0 = plain rect
uniform float uPixelScale;

// Rounded-box SDF (Inigo Quilez) - degenerates to a plain rectangle when
// uCornerRadiusPx is 0.
float RoundedBoxSDF(vec2 centeredPx, vec2 halfSizePx, float radiusPx) {
    vec2 d = abs(centeredPx) - (halfSizePx - vec2(radiusPx));
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0) - radiusPx;
}

void main() {
    float shapeAlpha;
    if (uCornerRadiusPx <= 0.0) {
        shapeAlpha = 1.0;
    } else {
        vec2 localPx = vUV * uSizePx;
        float dist = RoundedBoxSDF(localPx - uSizePx * 0.5, uSizePx * 0.5, uCornerRadiusPx);
        // ~1 physical pixel AA band - dist is in logical units, so the
        // band shrinks as uPixelScale grows (see PushConstants' comment).
        float aaWidth = 1.0 / uPixelScale;
        shapeAlpha = 1.0 - smoothstep(-aaWidth, aaWidth, dist);
    }
    gl_FragColor = vec4(uColor.rgb, uColor.a * shapeAlpha);
}
