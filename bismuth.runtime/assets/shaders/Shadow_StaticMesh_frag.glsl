#version 450

layout (set = 1, binding = 0) uniform sampler2D color_map;

// Data Transfer Object
layout(location = 1) in struct dto
{
	vec2 tex_coord;
} in_dto;

layout(location = 0) out vec4 out_color;

void main()
{
    float alpha = texture(color_map, in_dto.tex_coord).a;
    out_color = vec4(1.0, 1.0, 1.0, alpha);
    if(alpha < 0.5)
        discard;
}
