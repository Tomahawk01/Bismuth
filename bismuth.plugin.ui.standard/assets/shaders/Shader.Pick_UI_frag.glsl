#version 450

// =========================================================
// Inputs
// =========================================================

layout(set = 0, binding = 0) uniform per_frame_ubo
{
    mat4 projection;
	mat4 view;
} sui_pick_frame_ubo;

layout(set = 1, binding = 0) uniform per_group_ubo
{
    vec3 id_color;
} sui_pick_group_ubo;

layout(push_constant) uniform per_draw_ubo
{
	// Only guaranteed a total of 128 bytes
	mat4 model; // 64 bytes
} sui_pick_draw_ubo;

// =========================================================
// Outputs
// =========================================================
layout(location = 0) out vec4 out_color;

void main()
{
    out_color =  vec4(sui_pick_group_ubo.id_color, 1.0);
}
