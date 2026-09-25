//iris_replace_glsl_version
precision highp float;
uniform vec4 p_color;

uniform sampler2D p_texture;
uniform int p_texture_provided;

uniform vec2 p_pos;
uniform vec2 p_size;

in vec2 v_uv;

out vec4 FragColor;
void main() {
    if (p_texture_provided == 1) {
        FragColor = texture(p_texture, v_uv);
    } else {
        FragColor = p_color;
    }
}
