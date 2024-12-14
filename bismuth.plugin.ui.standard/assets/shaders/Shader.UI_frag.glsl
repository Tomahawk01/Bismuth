#version 450

struct ui_properties
{
    vec4 diffuse_color;
};

// =========================================================
// Inputs
// =========================================================

layout(set = 0, binding = 0) uniform per_frame_ubo
{
    mat4 projection;
	mat4 view;
} sui_frame_ubo;

layout(set = 1, binding = 0) uniform per_group_ubo
{
    ui_properties properties;
} sui_group_ubo;

layout(set = 1, binding = 1) uniform texture2D atlas_texture;
layout(set = 1, binding = 2) uniform sampler atlas_sampler;

layout(push_constant) uniform per_draw_ubo
{
	mat4 model; // 64 bytes
} sui_draw_ubo;

// Data Transfer Object from vertex shader
layout(location = 0) in struct dto
{
	vec2 tex_coord;
} in_dto;

// =========================================================
// Outputs
// =========================================================
layout(location = 0) out vec4 out_color;

void main()
{
    out_color = sui_group_ubo.properties.diffuse_color * texture(sampler2D(atlas_texture, atlas_sampler), in_dto.tex_coord);
}
