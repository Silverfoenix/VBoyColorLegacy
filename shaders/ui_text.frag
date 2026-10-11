#version 110

// Port of ui_text.frag. The glyph atlas is uploaded as GL_LUMINANCE, so
// .r carries the coverage value directly. Gamma-throughout - see
// ui_solid.frag's header comment.

varying vec2 vUV;

uniform sampler2D uAtlas;
uniform vec4 uColor;

void main() {
    float a = texture2D(uAtlas, vUV).r;
    gl_FragColor = vec4(uColor.rgb, uColor.a * a);
}
