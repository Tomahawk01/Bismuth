#include "basset_material_serializer.h"

#include "assets/basset_types.h"
#include "containers/darray.h"
#include "core_render_types.h"
#include "logger.h"
#include "math/bmath.h"
#include "parsers/bson_parser.h"
#include "strings/bname.h"
#include "strings/bstring.h"
#include "utils/render_type_utils.h"

// The current version of the material file format
#define MATERIAL_FILE_VERSION 3

#define INPUT_BASE_COLOR "base_color"
#define INPUT_NORMAL "normal"
#define INPUT_METALLIC "metallic"
#define INPUT_ROUGHNESS "roughness"
#define INPUT_AO "ao"
#define INPUT_MRA "mra"
#define INPUT_EMISSIVE "emissive"
#define INPUT_DUDV "dudv"

#define INPUT_MAP "map"
#define INPUT_VALUE "value"
#define INPUT_ENABLED "enabled"

#define INPUT_MAP_RESOURCE_NAME "resource_name"
#define INPUT_MAP_PACKAGE_NAME "package_name"
#define INPUT_MAP_SAMPLER_NAME "sampler_name"
#define INPUT_MAP_SOURCE_CHANNEL "source_channel"

#define SAMPLERS "samplers"

static b8 extract_input_map_channel_or_float(const bson_object* inputs_obj, const char* input_name, b8* out_enabled, bmaterial_texture_input* out_texture, texture_channel* out_source_channel, f32* out_value, f32 default_value);
static b8 extract_input_map_channel_or_vec4(const bson_object* inputs_obj, const char* input_name, b8* out_enabled, bmaterial_texture_input* out_texture, vec4* out_value, vec4 default_value);
static b8 extract_input_map_channel_or_vec3(const bson_object* inputs_obj, const char* input_name, b8* out_enabled, bmaterial_texture_input* out_texture, vec3* out_value, vec3 default_value);

static void add_map_obj(bson_object* base_obj, const char* source_channel, bmaterial_texture_input* texture);
static b8 extract_map(const bson_object* map_obj, bmaterial_texture_input* out_texture, texture_channel* out_source_channel);

const char* basset_material_serialize(const basset* asset)
{
    if (!asset)
    {
        BERROR("basset_material_serialize requires a valid pointer to a material");
        return 0;
    }

    bson_tree tree = {0};
    // The root of the tree
    tree.root = bson_object_create();

    basset_material* material = (basset_material*)asset;

    // Format version
    bson_object_value_add_int(&tree.root, "version", MATERIAL_FILE_VERSION);

    // Material type
    bson_object_value_add_string(&tree.root, "type", bmaterial_type_to_string(material->type));

    // Material model
    bson_object_value_add_string(&tree.root, "model", bmaterial_model_to_string(material->model));

    // Various flags
    bson_object_value_add_boolean(&tree.root, "has_transparency", material->has_transparency);
    bson_object_value_add_boolean(&tree.root, "double_sided", material->double_sided);
    bson_object_value_add_boolean(&tree.root, "recieves_shadow", material->recieves_shadow);
    bson_object_value_add_boolean(&tree.root, "casts_shadow", material->casts_shadow);
    bson_object_value_add_boolean(&tree.root, "use_vertex_color_as_base_color", material->use_vertex_color_as_base_color);

    // Material inputs
    bson_object inputs = bson_object_create();

    // Properties and maps used in all material types
    // Base color
    {
        bson_object base_color = bson_object_create();
        if (material->base_color_map.resource_name)
        {
            add_map_obj(&base_color, 0, &material->base_color_map);
        }
        else
        {
            bson_object_value_add_vec4(&base_color, INPUT_VALUE, material->base_color);
        }
        bson_object_value_add_object(&inputs, INPUT_BASE_COLOR, base_color);
    }

    // Normal
    {
        bson_object normal = bson_object_create();
        if (material->normal_map.resource_name)
        {
            add_map_obj(&normal, 0, &material->normal_map);
        }
        else
        {
            bson_object_value_add_vec3(&normal, INPUT_VALUE, material->normal);
        }
        bson_object_value_add_boolean(&normal, INPUT_ENABLED, material->normal_enabled);
        bson_object_value_add_object(&inputs, INPUT_NORMAL, normal);
    }

    // Properties and maps only used in standard materials
    if (material->type == BMATERIAL_TYPE_STANDARD)
    {
        // Metallic
        {
            bson_object metallic = bson_object_create();
            if (material->metallic_map.resource_name)
            {
                const char* channel = texture_channel_to_string(material->metallic_map_source_channel);
                add_map_obj(&metallic, channel, &material->metallic_map);
            }
            else
            {
                bson_object_value_add_float(&metallic, INPUT_VALUE, material->metallic);
            }
            bson_object_value_add_object(&inputs, INPUT_METALLIC, metallic);
        }

        // Ambient Occlusion
        {
            bson_object roughness = bson_object_create();
            if (material->roughness_map.resource_name)
            {
                const char* channel = texture_channel_to_string(material->roughness_map_source_channel);
                add_map_obj(&roughness, channel, &material->roughness_map);
            }
            else
            {
                bson_object_value_add_float(&roughness, INPUT_VALUE, material->roughness);
            }
            bson_object_value_add_object(&inputs, INPUT_ROUGHNESS, roughness);
        }

        // Ambient occlusion
        {
            bson_object ao = bson_object_create();
            if (material->ambient_occlusion_map.resource_name)
            {
                const char* channel = texture_channel_to_string(material->ambient_occlusion_map_source_channel);
                add_map_obj(&ao, channel, &material->ambient_occlusion_map);
            }
            else
            {
                bson_object_value_add_float(&ao, INPUT_VALUE, material->ambient_occlusion);
            }
            bson_object_value_add_boolean(&ao, INPUT_ENABLED, material->ambient_occlusion_enabled);
            bson_object_value_add_object(&inputs, INPUT_AO, ao);
        }

        // Metallic/roughness/ao combined value (mra) - only written out if used
        if (material->use_mra)
        {
            bson_object mra = bson_object_create();
            if (material->mra_map.resource_name)
            {
                add_map_obj(&mra, 0, &material->mra_map);
            }
            else
            {
                bson_object_value_add_vec3(&mra, INPUT_VALUE, material->mra);
            }
            bson_object_value_add_object(&inputs, INPUT_MRA, mra);
        }

        // Emissive
        {
            bson_object emissive = bson_object_create();
            if (material->emissive_map.resource_name)
            {
                add_map_obj(&emissive, 0, &material->emissive_map);
            }
            else
            {
                bson_object_value_add_vec4(&emissive, INPUT_VALUE, material->emissive);
            }
            bson_object_value_add_boolean(&emissive, INPUT_ENABLED, material->emissive_enabled);
            bson_object_value_add_object(&inputs, INPUT_EMISSIVE, emissive);
        }
    }

    // Properties only used in water materials
    if (material->type == BMATERIAL_TYPE_WATER)
    {
        // Top-level material properties
        bson_object_value_add_float(&tree.root, "tiling", material->tiling);
        bson_object_value_add_float(&tree.root, "wave_strength", material->wave_strength);
        bson_object_value_add_float(&tree.root, "wave_speed", material->wave_speed);

        // Besides normal, DUDV is also configurable
        {
            bson_object dudv = bson_object_create();
            if (material->dudv_map.resource_name)
            {
                add_map_obj(&dudv, 0, &material->dudv_map);
                bson_object_value_add_object(&inputs, INPUT_DUDV, dudv);
            }
        }
    }

    bson_object_value_add_object(&tree.root, "inputs", inputs);

    // Samplers
    if (material->custom_samplers && material->custom_sampler_count)
    {
        bson_array samplers_array = bson_array_create();

        // Each sampler
        for (u32 i = 0; i < material->custom_sampler_count; ++i)
        {
            bmaterial_sampler_config* custom_sampler = &material->custom_samplers[i];

            bson_object sampler = bson_object_create();

            bson_object_value_add_string(&sampler, "name", bname_string_get(custom_sampler->name));

            // Filtering
            bson_object_value_add_string(&sampler, "filter_min", texture_filter_mode_to_string(custom_sampler->filter_min));
            bson_object_value_add_string(&sampler, "filter_mag", texture_filter_mode_to_string(custom_sampler->filter_mag));

            // Repeats
            bson_object_value_add_string(&sampler, "repeat_u", texture_repeat_to_string(custom_sampler->repeat_u));
            bson_object_value_add_string(&sampler, "repeat_v", texture_repeat_to_string(custom_sampler->repeat_v));
            bson_object_value_add_string(&sampler, "repeat_w", texture_repeat_to_string(custom_sampler->repeat_w));

            // Add the sampler object to the array
            bson_array_value_add_object(&samplers_array, sampler);
        }

        // Add the samplers array to the root object
        bson_object_value_add_array(&tree.root, SAMPLERS, samplers_array);
    }

    // Tree is built, output it to a string
    const char* serialized = bson_tree_to_string(&tree);

    // Cleanup the tree
    bson_tree_cleanup(&tree);

    // Verify the result
    if (!serialized)
    {
        BERROR("Failed to output serialized material bson structure to string. See logs for details");
        return 0;
    }

    return serialized;
}

b8 basset_material_deserialize(const char* file_text, basset* out_asset)
{
    if (!file_text || !out_asset)
    {
        BERROR("basset_material_deserialize requires valid pointers to file_text and out_asset");
        return false;
    }

    bson_tree tree = {0};
    if (!bson_tree_from_string(file_text, &tree))
    {
        BERROR("Failed to parse material file. See logs for details");
        return 0;
    }

    b8 success = false;
    basset_material* out_material = (basset_material*)out_asset;

    // Material type
    const char* type_str = 0;
    if (!bson_object_property_value_get_string(&tree.root, "type", &type_str))
    {
        BERROR("failed to obtain type from material file, which is a required field");
        goto cleanup;
    }
    out_material->type = string_to_bmaterial_type(type_str);

    // Material model. Optional, defaults to PBR
    const char* model_str = 0;
    if (!bson_object_property_value_get_string(&tree.root, "model", &model_str))
        out_material->model = BMATERIAL_MODEL_PBR;
    else
        out_material->model = string_to_bmaterial_model(model_str);

    // Format version
    i64 file_format_version = 0;
    if (!bson_object_property_value_get_int(&tree.root, "version", &file_format_version))
    {
        BERROR("Unable to find file format version, a required field. Material will not be processed");
        goto cleanup;
    }
    // Validate version
    if (file_format_version != MATERIAL_FILE_VERSION)
    {
        if (file_format_version < 3)
        {
            BERROR("Material file format version %u no longer supported. File should be manually converted to at least version %u. Material will not be processed", file_format_version, MATERIAL_FILE_VERSION);
            goto cleanup;
        }

        if (file_format_version > MATERIAL_FILE_VERSION)
        {
            BERROR("Cannot process a file material version that is newer (%u) than the current version (%u)!", file_format_version, MATERIAL_FILE_VERSION);
            goto cleanup;
        }
    }

    // Various flags - fall back to defaults if not provided
    if (!bson_object_property_value_get_bool(&tree.root, "has_transparency", &out_material->has_transparency))
        out_material->has_transparency = false;
    if (!bson_object_property_value_get_bool(&tree.root, "double_sided", &out_material->double_sided))
        out_material->double_sided = false;
    if (!bson_object_property_value_get_bool(&tree.root, "recieves_shadow", &out_material->recieves_shadow))
        out_material->recieves_shadow = out_material->model != BMATERIAL_MODEL_UNLIT;
    if (!bson_object_property_value_get_bool(&tree.root, "casts_shadow", &out_material->casts_shadow))
        out_material->casts_shadow = out_material->model != BMATERIAL_MODEL_UNLIT;
    if (!bson_object_property_value_get_bool(&tree.root, "use_vertex_color_as_base_color", &out_material->use_vertex_color_as_base_color))
        out_material->use_vertex_color_as_base_color = false;

    // Properties only used in water materials
    if (out_material->type == BMATERIAL_TYPE_WATER)
    {
        // Top-level material properties - use defaults if not provided
        if (!bson_object_property_value_get_float(&tree.root, "tiling", &out_material->tiling))
            out_material->tiling = 0.25f;
        if (!bson_object_property_value_get_float(&tree.root, "wave_strength", &out_material->wave_strength))
            out_material->wave_strength = 0.02f;
        if (!bson_object_property_value_get_float(&tree.root, "wave_speed", &out_material->wave_speed))
            out_material->wave_speed = 0.03f;
    }

    // Extract inputs. The array of inputs is required, but the actual inputs themselves are optional
    {
        bson_object inputs_obj = {0};
        if (bson_object_property_value_get_object(&tree.root, "inputs", &inputs_obj))
        {
            u32 input_count = 0;

            // Get known inputs
            // base_color
            if (extract_input_map_channel_or_vec4(&inputs_obj, "base_color", 0, &out_material->base_color_map, &out_material->base_color, vec4_one()))
            {
                // Only count values actually included in config, as a validation check later
                input_count++;
            }

            // normal
            if (extract_input_map_channel_or_vec3(&inputs_obj, INPUT_NORMAL, &out_material->normal_enabled, &out_material->normal_map, &out_material->normal, (vec3){0.0f, 0.0f, 1.0f}))
            {
                // Only count values actually included in config, as a validation check later
                input_count++;
            }

            // Inputs only used in standard materials
            if (out_material->type == BMATERIAL_TYPE_STANDARD)
            {
                // mra
                if (extract_input_map_channel_or_vec3(&inputs_obj, "mra", 0, &out_material->mra_map, &out_material->mra, (vec3){0.0f, 0.5f, 1.0f}))
                {
                    // Only count values actually included in config, as a validation check later
                    input_count++;
                    // Flag to use MRA
                    out_material->use_mra = true;
                }

                // metallic
                if (extract_input_map_channel_or_float(&inputs_obj, "metallic", 0, &out_material->metallic_map, &out_material->metallic_map_source_channel, &out_material->metallic, 0.0f))
                {
                    // Only count values actually included in config, as a validation check later
                    input_count++;
                }

                // roughness
                if (extract_input_map_channel_or_float(&inputs_obj, "roughness", 0, &out_material->roughness_map, &out_material->roughness_map_source_channel, &out_material->roughness, 0.5f))
                {
                    // Only count values actually included in config, as a validation check later
                    input_count++;
                }

                // ao
                if (extract_input_map_channel_or_float(&inputs_obj, "ao", &out_material->ambient_occlusion_enabled, &out_material->ambient_occlusion_map, &out_material->ambient_occlusion_map_source_channel, &    out_material->ambient_occlusion, 1.0f))
                {
                    // Only count values actually included in config, as a validation check later
                    input_count++;
                }

                // emissive
                if (extract_input_map_channel_or_vec4(&inputs_obj, "emissive", &out_material->emissive_enabled, &out_material->emissive_map, &out_material->emissive, vec4_zero()))
                {
                    // Only count values actually included in config, as a validation check later
                    input_count++;
                }
            }

            // Inputs only used in water materials
            if (out_material->type == BMATERIAL_TYPE_WATER)
            {
                // Besides normal, DUDV is also configurable
                if (extract_input_map_channel_or_vec4(&inputs_obj, INPUT_DUDV, 0, &out_material->dudv_map, 0, vec4_zero()))
                {
                    // Only count values actually included in config, as a validation check later
                    input_count++;
                }
            }

            if (input_count < 1)
                BWARN("This material has no inputs. Why would you do that?");
        }
    }

    // Extract samplers
    {
        bson_array samplers_array = {0};
        if (bson_object_property_value_get_object(&tree.root, "samplers", &samplers_array))
        {
            if (bson_array_element_count_get(&samplers_array, &out_material->custom_sampler_count))
            {
                out_material->custom_samplers = darray_create(bmaterial_sampler_config);
                for (u32 i = 0; i < out_material->custom_sampler_count; ++i)
                {
                    bson_object sampler = {0};
                    if (bson_array_element_value_get_object(&samplers_array, i, &sampler))
                    {
                        // Extract sampler attributes
                        bmaterial_sampler_config custom_sampler = {0};

                        // name
                        if (!bson_object_property_value_get_string_as_bname(&sampler, "name", &custom_sampler.name))
                        {
                            BERROR("name, a required map field, was not found. Skipping sampler");
                            continue;
                        }

                        // The rest of the fields are all optional. Setup some defaults
                        custom_sampler.filter_mag = custom_sampler.filter_min = TEXTURE_FILTER_MODE_LINEAR;
                        custom_sampler.repeat_u = custom_sampler.repeat_v = custom_sampler.repeat_w = TEXTURE_REPEAT_REPEAT;
                        const char* value_str = 0;

                        // filters
                        // "filter" applies both. If it exists then set both
                        if (bson_object_property_value_get_string(&sampler, "filter", &value_str))
                        {
                            custom_sampler.filter_min = custom_sampler.filter_mag = string_to_texture_filter_mode(value_str);
                            string_free(value_str);
                        }
                        // Individual min/max overrides higher-level filter
                        if (bson_object_property_value_get_string(&sampler, "filter_min", &value_str))
                        {
                            custom_sampler.filter_min = string_to_texture_filter_mode(value_str);
                            string_free(value_str);
                        }
                        if (bson_object_property_value_get_string(&sampler, "filter_mag", &value_str))
                        {
                            custom_sampler.filter_mag = string_to_texture_filter_mode(value_str);
                            string_free(value_str);
                        }

                        // repeats
                        // "repeat" applies to all 3
                        if (bson_object_property_value_get_string(&sampler, "repeat", &value_str))
                        {
                            custom_sampler.repeat_u = custom_sampler.repeat_v = custom_sampler.repeat_w = string_to_texture_repeat(value_str);
                            string_free(value_str);
                        }
                        // Individual u/v/w overrides higher-level repeat
                        if (bson_object_property_value_get_string(&sampler, "repeat_u", &value_str))
                        {
                            custom_sampler.repeat_u = string_to_texture_repeat(value_str);
                            string_free(value_str);
                        }
                        if (bson_object_property_value_get_string(&sampler, "repeat_v", &value_str))
                        {
                            custom_sampler.repeat_v = string_to_texture_repeat(value_str);
                            string_free(value_str);
                        }
                        if (bson_object_property_value_get_string(&sampler, "repeat_w", &value_str))
                        {
                            custom_sampler.repeat_w = string_to_texture_repeat(value_str);
                            string_free(value_str);
                        }
                        // Push to array
                        darray_push(out_material->custom_samplers, custom_sampler);
                    }
                }
            }
        }
    }

    // Done parsing!
    success = true;
cleanup:
    bson_tree_cleanup(&tree);

    return success;
}

static b8 extract_input_map_channel_or_float(const bson_object* inputs_obj, const char* input_name, b8* out_enabled, bmaterial_texture_input* out_texture, texture_channel* out_source_channel, f32* out_value, f32 default_value)
{
    bson_object input = {0};
    b8 input_found = false;
    if (bson_object_property_value_get_object(inputs_obj, input_name, &input))
    {
        bson_object map_obj = {0};
        if (out_enabled)
            bson_object_property_value_get_bool(&input, INPUT_ENABLED, out_enabled);

        b8 has_map = bson_object_property_value_get_object(&input, INPUT_MAP, &map_obj);
        b8 has_value = out_value ? bson_object_property_value_get_float(&input, INPUT_VALUE, out_value) : false;
        if (has_map && has_value)
        {
            BWARN("Input '%s' specified both a value and a map. The map will be used", input_name);
            if (out_value)
                *out_value = default_value;
            has_value = false;
            input_found = true;
        }
        else if (!has_map && !has_value)
        {
            BWARN("Input '%s' specified neither a value nor a map. A default value will be used", input_name);
            if (out_value)
                *out_value = default_value;
            has_value = true;
        }
        else
        {
            // Valid case where only a map OR value was provided. Only count provided inputs
            input_found = true;
        }

        // Texture input
        if (has_map)
        {
            if (!extract_map(&map_obj, out_texture, out_source_channel))
                return false;
        }
    }
    else
    {
        // If nothing is specified, use the default for this
        if (out_value)
            *out_value = default_value;
    }
    
    return input_found;
}

static b8 extract_input_map_channel_or_vec4(const bson_object* inputs_obj, const char* input_name, b8* out_enabled, bmaterial_texture_input* out_texture, vec4* out_value, vec4 default_value)
{
    bson_object input = {0};
    b8 input_found = false;
    if (bson_object_property_value_get_object(inputs_obj, input_name, &input))
    {
        bson_object map_obj = {0};
        if (out_enabled)
            bson_object_property_value_get_bool(&input, INPUT_ENABLED, out_enabled);

        b8 has_map = bson_object_property_value_get_object(&input, INPUT_MAP, &map_obj);
        b8 has_value = out_value ? bson_object_property_value_get_vec4(&input, INPUT_VALUE, out_value) : false;
        if (has_map && has_value)
        {
            BWARN("Input '%s' specified both a value and a map. The map will be used", input_name);
            if (out_value)
                *out_value = default_value;
            has_value = false;
            input_found = true;
        }
        else if (!has_map && !has_value)
        {
            BWARN("Input '%s' specified neither a value nor a map. A default value will be used", input_name);
            if (out_value)
                *out_value = default_value;
            has_value = true;
        }
        else
        {
            // Valid case where only a map OR value was provided. Only count provided inputs
            input_found = true;
        }

        // Texture input
        if (has_map)
        {
            if (!extract_map(&map_obj, out_texture, 0))
                return false;
        }
    }
    else
    {
        // If nothing is specified, use the default for this
        if (out_value)
            *out_value = default_value;
    }

    return input_found;
}

static b8 extract_input_map_channel_or_vec3(const bson_object* inputs_obj, const char* input_name, b8* out_enabled, bmaterial_texture_input* out_texture, vec3* out_value, vec3 default_value)
{
    bson_object input = {0};
    b8 input_found = false;
    if (bson_object_property_value_get_object(inputs_obj, input_name, &input))
    {
        bson_object map_obj = {0};
        if (out_enabled)
            bson_object_property_value_get_bool(&input, INPUT_ENABLED, out_enabled);

        b8 has_map = bson_object_property_value_get_object(&input, INPUT_MAP, &map_obj);
        b8 has_value = out_value ? bson_object_property_value_get_vec3(&input, INPUT_VALUE, out_value) : false;
        if (has_map && has_value)
        {
            BWARN("Input '%s' specified both a value and a map. The map will be used.", input_name);
            if (out_value)
                *out_value = default_value;
            has_value = false;
            input_found = true;
        }
        else if (!has_map && !has_value)
        {
            BWARN("Input '%s' specified neither a value nor a map. A default value will be used.", input_name);
            if (out_value)
                *out_value = default_value;
            has_value = true;
        }
        else
        {
            // Valid case where only a map OR value was provided. Only count provided inputs
            input_found = true;
        }

        // Texture input
        if (has_map)
        {
            if (!extract_map(&map_obj, out_texture, 0))
                return false;
        }
    }
    else
    {
        // If nothing is specified, use the default for this
        if (out_value)
            *out_value = default_value;
    }

    return input_found;
}

static void add_map_obj(bson_object* base_obj, const char* source_channel, bmaterial_texture_input* texture)
{
    // Add map object
    bson_object map_obj = bson_object_create();
    bson_object_value_add_bname_as_string(&map_obj, INPUT_MAP_RESOURCE_NAME, texture->resource_name);
    // Package name. Optional
    if (texture->package_name)
        bson_object_value_add_bname_as_string(&map_obj, INPUT_MAP_PACKAGE_NAME, texture->package_name);

    // Sampler name. Optional
    if (texture->sampler_name)
        bson_object_value_add_bname_as_string(&map_obj, INPUT_MAP_SAMPLER_NAME, texture->sampler_name);

    // Source channel, if provided
    if (source_channel)
        bson_object_value_add_string(&map_obj, INPUT_MAP_SOURCE_CHANNEL, source_channel);

    bson_object_value_add_object(base_obj, INPUT_MAP, map_obj);
}

static b8 extract_map(const bson_object* map_obj, bmaterial_texture_input* out_texture, texture_channel* out_source_channel)
{
    // Extract the resource_name. Required
    if (!bson_object_property_value_get_string_as_bname(map_obj, INPUT_MAP_RESOURCE_NAME, &out_texture->resource_name))
    {
        BERROR("input map.resource_name is required");
        return false;
    }

    // Attempt to extract package name, optional
    if (!bson_object_property_value_get_string_as_bname(map_obj, INPUT_MAP_PACKAGE_NAME, &out_texture->package_name))
        out_texture->package_name = INVALID_BNAME;

    // Optional property, so it doesn't matter if we get it or not
    if (!bson_object_property_value_get_string_as_bname(map_obj, INPUT_MAP_SAMPLER_NAME, &out_texture->sampler_name))
        out_texture->sampler_name = INVALID_BNAME;

    if (out_source_channel)
    {
        // For floats, a source channel must be chosen. Default is red
        const char* channel = 0;
        bson_object_property_value_get_string(map_obj, INPUT_MAP_SOURCE_CHANNEL, &channel);
        if (channel)
        {
            *out_source_channel = string_to_texture_channel(channel);
        }
        else
        {
            // Use default of r
            *out_source_channel = TEXTURE_CHANNEL_R;
        }
    }

    return true;
}
