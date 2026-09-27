//iris_replace_glsl_version
precision highp float;
layout (location = 0) in vec2 a_pos;

out vec2 v_uv;

void main() {
    vec2 normalized = a_pos;
    vec2 ndc = vec2(
        normalized.x * 2.0 - 1.0,
        1.0 - normalized.y * 2.0
    );
    v_uv = a_pos;
    gl_Position = vec4(ndc, 1.0, 1.0);
}
