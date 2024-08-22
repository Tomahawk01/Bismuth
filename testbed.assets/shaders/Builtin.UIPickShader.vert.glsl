#version 450

layout(location = 0) in vec2 in_position;
layout(location = 1) in vec2 in_texcoord;

layout(set = 0, binding = 0) uniform global_uniform_object
{
    mat4 projection;
	mat4 view;
} global_ubo;

layout(push_constant) uniform push_constants
{
	mat4 model;
} u_push_constants;

void main()
{
	gl_Position = global_ubo.projection * global_ubo.view * u_push_constants.model * vec4(in_position, 0.0, 1.0);
}
