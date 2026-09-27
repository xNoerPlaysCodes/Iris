//iris_replace_glsl_version
precision highp float;

uniform sampler2D p_texture;

in vec2 v_uv;
out vec4 FragColor;

void main() {
    FragColor = texture(p_texture, vec2(v_uv.x, 1. - v_uv.y));
}
