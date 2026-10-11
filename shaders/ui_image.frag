#version 110

// Port of ui_image.frag. textureSize() -> uTexSize uniform; fwidth() ->
// the vUvPerPx varying (exact constant for this quad - see ui.vert).
// Gamma-throughout: sampled texels and the tint both pass through
// unchanged (Vulkan's decode-on-read / encode-on-store round-trips were
// identity); the tint multiply and alpha fade behave as before.

varying vec2 vUV;
varying vec2 vUvPerPx;

uniform sampler2D uImage;
uniform vec2 uTexSize;
uniform vec4 uColor;

// Anti-aliased pixel-perfect sampling (adapted from shadertoy.com/view/
// ltfXWS) - requires LINEAR filtering. vUvPerPx stands in for fwidth.
vec4 SamplePixelPerfectAA(sampler2D tex, vec2 uv) {
    vec2 uvTexSpace = uv * uTexSize;
    vec2 seam = floor(uvTexSpace + 0.5);
    uvTexSpace = (uvTexSpace - seam) / vUvPerPx + seam;
    uvTexSpace = clamp(uvTexSpace, seam - 0.5, seam + 0.5);
    return texture2D(tex, uvTexSpace / uTexSize);
}

void main() {
    gl_FragColor = SamplePixelPerfectAA(uImage, vUV);
    // vColor.a: fade a drawn region; vColor.rgb: tint it (white = no-op).
    gl_FragColor.a *= uColor.a;
    gl_FragColor.rgb *= uColor.rgb;
}
