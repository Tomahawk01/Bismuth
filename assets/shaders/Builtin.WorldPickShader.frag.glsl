#version 450

layout(location = 0) out vec4 out_color;

layout(set = 1, binding = 0) uniform local_uniform_object
{
    vec3 id_color;
} object_ubo;

void main()
{
    out_color =  vec4(object_ubo.id_color, 1.0);
}
