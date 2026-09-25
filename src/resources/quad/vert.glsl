//iris_replace_glsl_version
precision highp float;
layout (location = 0) in vec2 a_pos;
uniform vec2 p_pos;
uniform vec2 p_size;

out vec2 v_uv;

const vec2 pos_offsets[5] = vec2[5](
    vec2(0.0,  0.0),
    vec2(0.5,  0.0),
    vec2(1.0,  0.0),
    vec2(0.0,  -0.5),
    vec2(0.0,  -1.0)
);

void main() {
    vec2 normalized = p_pos + a_pos * p_size;

    vec2 ndc = vec2(
        normalized.x * 2.0 - 1.0,
        1.0 - normalized.y * 2.0
    );

    v_uv = vec2(a_pos.x, a_pos.y);

    gl_Position = vec4(ndc.x + pos_offsets[gl_InstanceID].x, ndc.y + pos_offsets[gl_InstanceID].y, 1.0, 1.0);
}
