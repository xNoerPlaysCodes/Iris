//iris_replace_glsl_version
precision highp float;
layout (location = 0) in vec2 a_pos;
layout (location = 1) in vec2 i_pos;
layout (location = 2) in vec2 i_size;
layout (location = 3) in vec4 i_color;

out vec4 v_color;

out vec2 v_uv;

void main() {
    vec2 normalized = i_pos + a_pos * i_size;
    vec2 ndc = vec2(
        normalized.x * 2.0 - 1.0,
        1.0 - normalized.y * 2.0
    );
    v_uv = vec2(a_pos.x, a_pos.y);
    v_color = i_color;
    gl_Position = vec4(i_pos.xy, 1.0, 1.0);
}
