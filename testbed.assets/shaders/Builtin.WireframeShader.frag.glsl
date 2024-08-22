#version 450

layout(location = 0) out vec4 out_color;

// Data Transfer Object
layout(location = 1) in struct dto
{
	vec4 color;
} in_dto;

void main()
{
    out_color = in_dto.color;
}
