//iris_replace_glsl_version
precision highp float;
layout (location = 0) in vec2 a_pos;
uniform vec2 iris_pos;
uniform vec2 iris_size;

out vec2 v_uv;

void main() {
    vec2 normalized = iris_pos + a_pos * iris_size;

    vec2 ndc = vec2(
        normalized.x * 2.0 - 1.0,
        1.0 - normalized.y * 2.0
    );

    float uv_y = mix(a_pos.y, 1.0 - a_pos.y, 1);
    v_uv = vec2(a_pos.x, uv_y);

    gl_Position = vec4(ndc.x, ndc.y, 1.0, 1.0);
}
