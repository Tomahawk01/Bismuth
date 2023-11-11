#version 450

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_texcoord;
layout(location = 3) in vec4 in_color;
layout(location = 4) in vec3 in_tangent;

const int POINT_LIGHT_MAX = 10;

struct directional_light
{
    vec4 color;
    vec4 direction;
};

struct point_light
{
    vec4 color;
    vec4 position;
    // Usually 1, make sure denominator never gets smaller than 1
    float constant_f;
    // Reduces light intensity linearly
    float linear;
    // Makes light fall off slower at longer distances
    float quadratic;
    float padding;
};

layout(set = 0, binding = 0) uniform global_uniform_object
{
    mat4 projection;
	mat4 view;
	mat4 model;
	vec4 ambient_color;
	vec3 view_position;
	int mode;
	vec4 diffuse_color;
    directional_light dir_light;
    point_light p_lights[POINT_LIGHT_MAX];
    int num_p_lights;
    float shininess;
} global_ubo;

layout(location = 0) out int out_mode;

// Data Transfer Object
layout(location = 1) out struct dto
{
	vec4 ambient;
	vec2 tex_coord;
	vec3 normal;
	vec3 view_position;
	vec3 frag_position;
	vec4 color;
	vec3 tangent;
} out_dto;

void main()
{
	out_dto.tex_coord = in_texcoord;
	out_dto.color = in_color;
	// Fragment position in world space
	out_dto.frag_position = vec3(global_ubo.model * vec4(in_position, 1.0));
	// Copy the normal over
	mat3 m3_model = mat3(global_ubo.model);
	out_dto.normal = normalize(m3_model * in_normal);
	out_dto.tangent = normalize(m3_model * in_tangent);
	out_dto.ambient = global_ubo.ambient_color;
	out_dto.view_position = global_ubo.view_position;
    gl_Position = global_ubo.projection * global_ubo.view * global_ubo.model * vec4(in_position, 1.0);

	out_mode = global_ubo.mode;
}
