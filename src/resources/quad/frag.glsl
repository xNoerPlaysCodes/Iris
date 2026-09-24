//iris_replace_glsl_version
precision highp float;
uniform vec4 p_color;

uniform sampler2D texture_;
uniform int texture_provided;

out vec4 FragColor;
void main() {
    if (texture_provided == 1) {
        FragColor = texture(texture_, gl_FragCoord.xy);
    } else {
        FragColor = p_color;
    }
}
