#version 110

// Port of ui_image_rounded.frag - clips a pre-rendered buffer to rounded
// corners in one draw. Gamma-throughout (see ui_solid.frag).

varying vec2 vUV;

uniform sampler2D uImage;
uniform vec4 uColor;           // .a fades the whole composited buffer
uniform vec2 uSizePx;
uniform float uCornerRadiusPx;
uniform float uPixelScale;

float RoundedBoxSDF(vec2 centeredPx, vec2 halfSizePx, float radiusPx) {
    vec2 d = abs(centeredPx) - (halfSizePx - vec2(radiusPx));
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0) - radiusPx;
}

void main() {
    vec4 texColor = texture2D(uImage, vUV);
    vec2 localPx = vUV * uSizePx;
    float dist = RoundedBoxSDF(localPx - uSizePx * 0.5, uSizePx * 0.5, uCornerRadiusPx);
    float aaWidth = 1.0 / uPixelScale;
    float shapeAlpha = 1.0 - smoothstep(-aaWidth, aaWidth, dist);
    gl_FragColor = vec4(texColor.rgb, texColor.a * shapeAlpha * uColor.a);
}
