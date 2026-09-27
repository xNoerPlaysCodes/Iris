//iris_replace_glsl_version
precision highp float;
layout (location = 0) in vec2 a_pos;
layout (location = 1) in vec3 i_pos;
layout (location = 2) in vec2 i_size;
layout (location = 3) in vec4 i_color;

out vec4 v_color;

out vec2 v_uv;
out float v_z;

void main() {
    vec2 normalized = i_pos.xy + a_pos * i_size;
    vec3 ndc = vec3(
        normalized.x * 2.0 - 1.0,
        1.0 - normalized.y * 2.0,
        i_pos.z * 2.0 - 1.0
    );
    v_uv = a_pos;
    v_color = i_color;
    v_z = i_pos.z;
    gl_Position = vec4(ndc, 1.0);
}
