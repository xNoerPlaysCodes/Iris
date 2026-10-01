//iris_replace_glsl_version
precision highp float;

uniform sampler2D p_texture;
uniform int p_texture_provided;

in vec4 v_color;
in vec2 v_uv;
in float v_z;

out vec4 FragColor;
void main() {
    if (p_texture_provided == 1) {
        FragColor = texture(p_texture, v_uv);
    } else {
        FragColor = v_color;
    }
}
