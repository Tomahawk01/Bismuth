#version 450

layout(location = 0) out vec4 out_color;

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

const int MAX_POINT_LIGHTS = 10;

struct pbr_properties
{
    vec4 diffuse_color;
    vec3 padding;
    float shininess;
};

layout(set = 1, binding = 0) uniform instance_uniform_object
{
    directional_light dir_light; // TODO: make global
    point_light p_lights[MAX_POINT_LIGHTS];
    pbr_properties properties;
    int num_p_lights;
} instance_ubo;

// Samplers
const int SAMP_ALBEDO = 0;
const int SAMP_NORMAL = 1;
const int SAMP_METALLIC = 2;
const int SAMP_ROUGHNESS = 3;
const int SAMP_AO = 4;
const int SAMP_IBL_CUBE = 5;

const float PI = 3.14159265359;
// Samplers. albedo, normal, metallic, roughness, ao ...
layout(set = 1, binding = 1) uniform sampler2D samplers[6];
// IBL
layout(set = 1, binding = 1) uniform samplerCube cube_samplers[6];

layout(location = 0) flat in int in_mode;
// Data Transfer Object
layout(location = 1) in struct dto
{
    vec4 ambient;
	vec2 tex_coord;
	vec3 normal;
	vec3 view_position;
	vec3 frag_position;
    vec4 color;
	vec3 tangent;
} in_dto;

mat3 TBN;

// Based on a combination of GGX and Schlick-Beckmann approximation to calculate probability of overshadowing micro-facets
float geometry_schlick_ggx(float normal_dot_direction, float roughness)
{
    roughness += 1.0;
    float k = (roughness * roughness) / 8.0;
    return normal_dot_direction / (normal_dot_direction * (1.0 - k) + k);
}

vec3 calculate_point_light_radiance(point_light light, vec3 view_direction, vec3 frag_position_xyz);
vec3 calculate_directional_light_radiance(directional_light light, vec3 view_direction);
vec3 calculate_reflectance(vec3 albedo, vec3 normal, vec3 view_direction, vec3 light_direction, float metallic, float roughness, vec3 base_reflectivity, vec3 radiance);

void main()
{
    vec3 normal = in_dto.normal;
    vec3 tangent = in_dto.tangent;
    tangent = (tangent - dot(tangent, normal) *  normal);
    vec3 bitangent = cross(in_dto.normal, in_dto.tangent);
    TBN = mat3(tangent, bitangent, normal);

    // Update normal to use a sample from the normal map
    vec3 local_normal = 2.0 * texture(samplers[SAMP_NORMAL], in_dto.tex_coord).rgb - 1.0;
    normal = normalize(TBN * local_normal);

    vec4 albedo_samp = texture(samplers[SAMP_ALBEDO], in_dto.tex_coord);
    vec3 albedo = pow(albedo_samp.rgb, vec3(2.2));

    float metallic = texture(samplers[SAMP_METALLIC], in_dto.tex_coord).r;
    float roughness = texture(samplers[SAMP_ROUGHNESS], in_dto.tex_coord).r;
    float ao = texture(samplers[SAMP_AO], in_dto.tex_coord).r;

    // Calculate reflectance. If dia-electric (plastic) use base_reflectivity of 0.04, and if it's a metal use albedo color as base_reflectivity (metallic) 
    vec3 base_reflectivity = vec3(0.04); 
    base_reflectivity = mix(base_reflectivity, albedo, metallic);

    if(in_mode == 0 || in_mode == 1)
    {
        vec3 view_direction = normalize(in_dto.view_position - in_dto.frag_position);

        albedo += (vec3(1.0) * in_mode);         
        albedo = clamp(albedo, vec3(0.0), vec3(1.0));

        // Overall reflectance
        vec3 total_reflectance = vec3(0.0);

        // Directional light radiance
        {
            directional_light light = instance_ubo.dir_light;
            vec3 light_direction = normalize(-light.direction.xyz);
            vec3 radiance = calculate_directional_light_radiance(light, view_direction);

            total_reflectance += calculate_reflectance(albedo, normal, view_direction, light_direction, metallic, roughness, base_reflectivity, radiance);
        }

        // Point light radiance
        for(int i = 0; i < instance_ubo.num_p_lights; ++i)
        {
            point_light light = instance_ubo.p_lights[i];
            vec3 light_direction = normalize(light.position.xyz - in_dto.frag_position.xyz);
            vec3 radiance = calculate_point_light_radiance(light, view_direction, in_dto.frag_position.xyz);

            total_reflectance += calculate_reflectance(albedo, normal, view_direction, light_direction, metallic, roughness, base_reflectivity, radiance);
        }

        // Irradiance holds all the scene's indirect diffuse light
        vec3 irradiance = texture(cube_samplers[SAMP_IBL_CUBE], normal).rgb;

        // Combine irradiance with albedo and ambient occlusion. Also add in total accumulated reflectance
        vec3 ambient = irradiance * albedo * ao;
        vec3 color = ambient + total_reflectance;

        // HDR tonemapping
        color = color / (color + vec3(1.0));
        // Gamma correction
        color = pow(color, vec3(1.0 / 2.2));

        // Ensure the alpha is based on the albedo's original alpha  value
        out_color = vec4(color, albedo_samp.a);
    }
    else if(in_mode == 2)
    {
        out_color = vec4(abs(normal), 1.0);
    }
}

vec3 calculate_reflectance(vec3 albedo, vec3 normal, vec3 view_direction, vec3 light_direction, float metallic, float roughness, vec3 base_reflectivity, vec3 radiance)
{
    vec3 halfway = normalize(view_direction + light_direction);

    // This is based off the Cook-Torrance BRDF (Bidirectional Reflective Distribution Function)

    float roughness_sq = roughness*roughness;
    float roughness_sq_sq = roughness_sq * roughness_sq;
    float normal_dot_halfway = max(dot(normal, halfway), 0.0);
    float normal_dot_halfway_squared = normal_dot_halfway * normal_dot_halfway;
    float denom = (normal_dot_halfway_squared * (roughness_sq_sq - 1.0) + 1.0);
    denom = PI * denom * denom;
    float normal_distribution = (roughness_sq_sq / denom);

    // Geometry function which calculates self-shadowing on micro-facets (more pronounced on rough surfaces)
    float normal_dot_view_direction = max(dot(normal, view_direction), 0.0);
    // Scale the light by the dot product of normal and light_direction
    float normal_dot_light_direction = max(dot(normal, light_direction), 0.0);
    float ggx_0 = geometry_schlick_ggx(normal_dot_view_direction, roughness);
    float ggx_1 = geometry_schlick_ggx(normal_dot_light_direction, roughness);
    float geometry = ggx_1 * ggx_0;

    // Fresnel-Schlick approximation for the fresnel
    float cos_theta = max(dot(halfway, view_direction), 0.0);
    vec3 fresnel = base_reflectivity + (1.0 - base_reflectivity) * pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);

    // Take Normal distribution * geometry * fresnel and calculate specular reflection
    vec3 numerator = normal_distribution * geometry * fresnel;
    float denominator = 4.0 * max(dot(normal, view_direction), 0.0) + 0.0001; // prevent div by 0 
    vec3 specular = numerator / denominator;

    vec3 refraction_diffuse = vec3(1.0) - fresnel;
    refraction_diffuse *= 1.0 - metallic;	  

    // End result is the reflectance to be added to the overall, which is tracked by the caller
    return (refraction_diffuse * albedo / PI + specular) * radiance * normal_dot_light_direction;  
}

vec3 calculate_point_light_radiance(point_light light, vec3 view_direction, vec3 frag_position_xyz)
{
    // Per-light radiance based on the point light's attenuation
    float distance = length(light.position.xyz - frag_position_xyz);
    float attenuation = 1.0 / (light.constant_f + light.linear * distance + light.quadratic * (distance * distance));
    return light.color.rgb * attenuation;
}

vec3 calculate_directional_light_radiance(directional_light light, vec3 view_direction)
{
    // For directional lights, radiance is just the same as the light color itself
    return light.color.rgb;
}
