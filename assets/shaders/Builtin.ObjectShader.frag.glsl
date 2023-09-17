#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec4 out_color;

layout(set = 1, binding = 0) uniform local_uniform_object
{
    vec4 diffuse_color;
} object_ubo;

// Samplers
layout(set = 1, binding = 1) uniform sampler2D diffuse_sampler;
// Data Transfer Object
layout(location = 1) in struct dto
{
    vec2 tex_coord;
} in_dto;

void main()
{
    out_color = object_ubo.diffuse_color * texture(diffuse_sampler, in_dto.tex_coord);
}
