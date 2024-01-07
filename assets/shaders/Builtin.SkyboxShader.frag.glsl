#version 450

layout(location = 0) in vec3 tex_coord;
layout(location = 0) out vec4 out_color;

// Samplers
layout(set = 1, binding = 0) uniform samplerCube cube_texture;

void main()
{
    out_color = texture(cube_texture, tex_coord);
}
