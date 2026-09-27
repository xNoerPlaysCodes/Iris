//iris_replace_glsl_version
precision highp float;

uniform sampler2D p_texture;
uniform sampler2D p_depth;

in vec2 v_uv;
out vec4 FragColor;

void main() {
    // float d = texture(p_depth, v_uv).r;
    // FragColor = vec4(vec3(d), 1.0);
    FragColor = vec4(vec3(1.0) - texture(p_texture, v_uv).rgb, 1.0);
}
