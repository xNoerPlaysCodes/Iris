//iris_replace_glsl_version
precision highp float;

uniform sampler2D p_texture;

in vec2 v_uv;
out vec4 FragColor;

void main() {
    FragColor = vec4(vec3(1.0) - texture(p_texture, v_uv).rgb, 1.0);
}
