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

static b8 extract_input_map_channel_or_float(const bson_object* inputs_obj, const char* input_name, basset_material_texture_map map, basset_material_texture* out_texture, basset_material_texture_map_channel* out_source_channel, f32* out_value, f32 default_value);
static b8 extract_input_map_channel_or_vec4(const bson_object* inputs_obj, const char* input_name, basset_material_texture_map map, basset_material_texture* out_map, vec4* out_value, vec4 default_value);
static b8 extract_input_map_channel_or_vec3(const bson_object* inputs_obj, const char* input_name, basset_material_texture_map map, basset_material_texture* out_map, vec3* out_value, vec3 default_value);

const char* basset_material_serialize(const basset* asset)
{
    if (!asset)
    {
        BERROR("basset_material_serialize requires a valid pointer to a material");
        return 0;
    }

    bson_tree tree = {0};
    // The root of the tree
    tree.root.type = BSON_OBJECT_TYPE_OBJECT;

    basset_material* material = (basset_material*)asset;

    // Material type
    bson_object_value_add_string(&tree.root, "type", bmaterial_type_to_string(material->type));

    // Material name
    bson_object_value_add_string(&tree.root, "name", bname_string_get(material->base.name));

    // Format version
    bson_object_value_add_int(&tree.root, "version", MATERIAL_FILE_VERSION);

    // Material properties
    if (material->properties && material->property_count)
    {
        bson_array properties_array = {0};
        properties_array.type = BSON_OBJECT_TYPE_ARRAY;

        // Each property
        for (u32 i = 0; i < material->property_count; ++i)
        {
            basset_material_property* p = &material->properties[i];

            bson_object prop = {0};
            prop.type = BSON_OBJECT_TYPE_OBJECT;

            bson_object_value_add_string(&prop, "name", bname_string_get(p->name));
            bson_object_value_add_string(&prop, "type", shader_uniform_type_to_string(p->type));

            // Add value as string for vector types. Otherwise add as int or float
            switch (p->type)
            {
            case SHADER_UNIFORM_TYPE_FLOAT32_4:
            {
                const char* str = vec4_to_string(p->value.v4);
                bson_object_value_add_string(&prop, "value", str);
                string_free(str);
            } break;
            case SHADER_UNIFORM_TYPE_FLOAT32_3:
            {
                const char* str = vec3_to_string(p->value.v3);
                bson_object_value_add_string(&prop, "value", str);
                string_free(str);
            } break;
            case SHADER_UNIFORM_TYPE_FLOAT32_2:
            {
                const char* str = vec2_to_string(p->value.v2);
                bson_object_value_add_string(&prop, "value", str);
                string_free(str);
            } break;
            case SHADER_UNIFORM_TYPE_FLOAT32:
            {
                bson_object_value_add_float(&prop, "value", p->value.f32);
            } break;
            case SHADER_UNIFORM_TYPE_MATRIX_4:
            {
                const char* str = mat4_to_string(p->value.mat4);
                bson_object_value_add_string(&prop, "value", str);
                string_free(str);
            } break;
            case SHADER_UNIFORM_TYPE_UINT8:
            {
                // NOTE: Treat as i64 since that's all bson deals with
                bson_object_value_add_int(&prop, "value", p->value.u8);
            } break;
            case SHADER_UNIFORM_TYPE_UINT16:
            {
                // NOTE: Treat as i64 since that's all bson deals with
                bson_object_value_add_int(&prop, "value", p->value.u16);
            } break;
            case SHADER_UNIFORM_TYPE_UINT32:
            {
                // NOTE: Treat as i64 since that's all bson deals with
                bson_object_value_add_int(&prop, "value", p->value.u32);
            } break;
            case SHADER_UNIFORM_TYPE_INT8:
            {
                // NOTE: Treat as i64 since that's all bson deals with
                bson_object_value_add_int(&prop, "value", p->value.i8);
            } break;
            case SHADER_UNIFORM_TYPE_INT16:
            {
                // NOTE: Treat as i64 since that's all bson deals with
                bson_object_value_add_int(&prop, "value", p->value.i16);
            } break;
            case SHADER_UNIFORM_TYPE_INT32:
            {
                // NOTE: Treat as i64 since that's all bson deals with
                bson_object_value_add_int(&prop, "value", p->value.i32);
            } break;
            case SHADER_UNIFORM_TYPE_CUSTOM:
            {
                // TODO: Provide some sort of (de)serialization mechanic for custom property types
                BWARN("Custom type material properties cannot have a value serialized to file when written");
                bson_object_value_add_int(&prop, "size", p->size);
            } break;
            default:
                BERROR("Cannot serialize sampler or unknown type properties, as they are not valid");
                break;
            }

            // Add the property object to the array
            bson_array_value_add_object(&properties_array, prop);
        }

        // Add the properties array to the root object
        bson_object_value_add_object(&tree.root, "properties", properties_array);
    }

    // Material maps
    if (material->maps && material->map_count)
    {
        bson_array maps_array = {0};
        maps_array.type = BSON_OBJECT_TYPE_ARRAY;

        // Each map
        for (u32 i = 0; i < material->map_count; ++i)
        {
            basset_material_map* m = &material->maps[i];

            bson_object map = {0};
            map.type = BSON_OBJECT_TYPE_OBJECT;

            bson_object_value_add_string(&map, "name", bname_string_get(m->name));

            // Fully-qualified image asset name
            if (m->image_asset_name)
                bson_object_value_add_string(&map, "image_asset_name", bname_string_get(m->image_asset_name));

            // Filtering
            bson_object_value_add_string(&map, "filter_min", texture_filter_mode_to_string(m->filter_min));
            bson_object_value_add_string(&map, "filter_mag", texture_filter_mode_to_string(m->filter_mag));

            // Repeats
            bson_object_value_add_string(&map, "repeat_u", texture_repeat_to_string(m->repeat_u));
            bson_object_value_add_string(&map, "repeat_v", texture_repeat_to_string(m->repeat_v));
            bson_object_value_add_string(&map, "repeat_w", texture_repeat_to_string(m->repeat_w));

            // Add the map object to the array
            bson_array_value_add_object(&maps_array, map);
        }

        // Add the maps array to the root object
        bson_object_value_add_object(&tree.root, "maps", maps_array);
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

    // Extract top-level properties first
    const char* material_name_str = 0;
    if (!bson_object_property_value_get_string(&tree.root, "name", &material_name_str))
    {
        BERROR("failed to obtain name from material file, which is a required field");
        goto cleanup;
    }
    out_material->base.name = bname_create(material_name_str);
    string_free(material_name_str);

    // Material type
    const char* type_str = 0;
    if (!bson_object_property_value_get_string(&tree.root, "type", &type_str))
    {
        BERROR("failed to obtain type from material file, which is a required field");
        goto cleanup;
    }
    out_material->type = string_to_bmaterial_type(type_str);

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
            BERROR("Material file format version %u no longer supported. File should be manually converted to at least version 3. Material will not be processed", file_format_version);
            goto cleanup;
        }

        if (file_format_version > MATERIAL_FILE_VERSION)
        {
            BERROR("Cannot process a file material version that is newer than the current version!");
            goto cleanup;
        }
    }

    // Extract inputs. The array of inputs is required, but the actual inputs themselves are optional
    {
        bson_object inputs_obj = {0};
        if (bson_object_property_value_get_object(&tree.root, "inputs", &inputs_obj))
        {
            u32 input_count = 0;

            // Get known inputs
            // base_color
            if (extract_input_map_channel_or_vec4(&inputs_obj, "base_color", BASSET_MATERIAL_TEXTURE_MAP_BASE_COLOR, &out_material->base_color_map, &out_material->base_color, vec4_one()))
            {
                // Only count values actually included in config, as a validation check later
                input_count++;
            }

            // mra
            if (extract_input_map_channel_or_vec3(&inputs_obj, "mra", BASSET_MATERIAL_TEXTURE_MAP_MRA, &out_material->mra_map, &out_material->mra, (vec3){0.0f, 0.5f, 1.0f}))
            {
                // Only count values actually included in config, as a validation check later
                input_count++;
                // Flag to use MRA
                out_material->use_mra = true;
            }

            // metallic
            if (extract_input_map_channel_or_float(&inputs_obj, "metallic", BASSET_MATERIAL_TEXTURE_MAP_METALLIC, &out_material->metallic_map, &out_material->metallic_map_source_channel, &out_material->metallic, 0.0f))
            {
                // Only count values actually included in config, as a validation check later
                input_count++;
            }

            // roughness
            if (extract_input_map_channel_or_float(&inputs_obj, "roughness", BASSET_MATERIAL_TEXTURE_MAP_ROUGHNESS, &out_material->roughness_map, &out_material->roughness_map_source_channel, &out_material->roughness, 0.5f))
            {
                // Only count values actually included in config, as a validation check later
                input_count++;
            }

            // ao
            if (extract_input_map_channel_or_float(&inputs_obj, "ao", BASSET_MATERIAL_TEXTURE_MAP_AO, &out_material->ambient_occlusion_map, &out_material->ambient_occlusion_map_source_channel, &out_material->ambient_occlusion, 1.0f))
            {
                // Only count values actually included in config, as a validation check later
                input_count++;
            }

            // emissive
            if (extract_input_map_channel_or_vec4(&inputs_obj, "emissive", BASSET_MATERIAL_TEXTURE_MAP_EMISSIVE, &out_material->emissive_map, &out_material->emissive, vec4_zero()))
            {
                // Only count values actually included in config, as a validation check later
                input_count++;
            }

            if (input_count < 1)
                BWARN("This material has no inputs. Why would you do that?");
        }
    }

    // Extract samplers
    {
        bson_array maps_array = {0};
        if (bson_object_property_value_get_object(&tree.root, "maps", &maps_array))
        {
            if (bson_array_element_count_get(&maps_array, &out_material->map_count))
            {
                out_material->maps = darray_create(basset_material_map);
                for (u32 i = 0; i < out_material->map_count; ++i)
                {
                    bson_object map = {0};
                    if (bson_array_element_value_get_object(&maps_array, i, &map))
                    {
                        // Extract attributes
                        basset_material_map m = {0};

                        // name
                        const char* map_name_str = 0;
                        if (!bson_object_property_value_get_string(&map, "channel", &map_name_str))
                        {
                            BERROR("name, a required map field, was not found. Skipping map...");
                            continue;
                        }
                        m.name = bname_create(map_name_str);
                        string_free(map_name_str);

                        // The rest of the fields are all optional
                        const char* value_str = 0;

                        // filters
                        m.filter_min = m.filter_mag = TEXTURE_FILTER_MODE_LINEAR;
                        if (bson_object_property_value_get_string(&map, "filter_min", &value_str))
                        {
                            m.filter_min = string_to_texture_filter_mode(value_str);
                            string_free(value_str);
                        }
                        if (bson_object_property_value_get_string(&map, "filter_mag", &value_str))
                        {
                            m.filter_mag = string_to_texture_filter_mode(value_str);
                            string_free(value_str);
                        }

                        // repeats
                        m.repeat_u = m.repeat_v = m.repeat_w = TEXTURE_REPEAT_REPEAT;
                        if (bson_object_property_value_get_string(&map, "repeat_u", &value_str))
                        {
                            m.repeat_u = string_to_texture_repeat(value_str);
                            string_free(value_str);
                        }
                        if (bson_object_property_value_get_string(&map, "repeat_v", &value_str))
                        {
                            m.repeat_v = string_to_texture_repeat(value_str);
                            string_free(value_str);
                        }
                        if (bson_object_property_value_get_string(&map, "repeat_w", &value_str))
                        {
                            m.repeat_w = string_to_texture_repeat(value_str);
                            string_free(value_str);
                        }
                    }
                    // Push to maps array
                    darray_push(out_material->maps, map);
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

static b8 extract_input_map_channel_or_float(const bson_object* inputs_obj, const char* input_name, basset_material_texture_map map, basset_material_texture* out_texture, basset_material_texture_map_channel* out_source_channel, f32* out_value, f32 default_value)
{
    bson_object input = {0};
    b8 input_found = false;
    if (bson_object_property_value_get_object(inputs_obj, input_name, &input))
    {
        b8 has_map = bson_object_property_value_get_bname(&input, "map", &out_texture->resource_name);
        b8 has_value = bson_object_property_value_get_float(&input, "value", out_value);
        if (has_map && has_value)
        {
            BWARN("Input '%s' specified both a value and a map. The map will be used", input_name);
            *out_value = default_value;
            has_value = false;
            input_found = true;
        }
        else if (!has_map && !has_value)
        {
            BWARN("Input '%s' specified neither a value nor a map. A default value will be used", input_name);
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
            out_texture->map_name = bname_create(input_name);
            out_texture->map = map;
            // Optional property, so it doesn't matter if we get it or not
            out_texture->sampler_name = INVALID_BNAME;
            bson_object_property_value_get_bname(&input, "sampler", &out_texture->sampler_name);

            // For floats, a source channel must be chosen. Default is red
            const char* channel = 0;
            bson_object_property_value_get_string(&input, "source_channel", &channel);
            if (channel)
            {
                if (strings_equali(channel, "r"))
                {
                    *out_source_channel = BASSET_MATERIAL_TEXTURE_MAP_CHANNEL_R;
                }
                else if (strings_equali(channel, "g"))
                {
                    *out_source_channel = BASSET_MATERIAL_TEXTURE_MAP_CHANNEL_G;
                }
                else if (strings_equali(channel, "b"))
                {
                    *out_source_channel = BASSET_MATERIAL_TEXTURE_MAP_CHANNEL_B;
                }
                else if (strings_equali(channel, "a"))
                {
                    *out_source_channel = BASSET_MATERIAL_TEXTURE_MAP_CHANNEL_A;
                }
                else
                {
                    BWARN("Input '%s' specified an invalid source_channel '%s'. A default value will be used", input_name, channel);
                    *out_source_channel = BASSET_MATERIAL_TEXTURE_MAP_CHANNEL_R;
                }
            }
            else
            {
                // Use default of r
                *out_source_channel = BASSET_MATERIAL_TEXTURE_MAP_CHANNEL_R;
            }
        }
    }
    else
    {
        // If nothing is specified, use the default for this
        *out_value = default_value;
    }
    
    return input_found;
}

static b8 extract_input_map_channel_or_vec4(const bson_object* inputs_obj, const char* input_name, basset_material_texture_map map, basset_material_texture* out_map, vec4* out_value, vec4 default_value)
{
    bson_object input = {0};
    b8 input_found = false;
    if (bson_object_property_value_get_object(inputs_obj, input_name, &input))
    {
        b8 has_map = bson_object_property_value_get_bname(&input, "map", &out_map->resource_name);
        b8 has_value = bson_object_property_value_get_vec4(&input, "value", out_value);
        if (has_map && has_value)
        {
            BWARN("Input '%s' specified both a value and a map. The map will be used", input_name);
            *out_value = default_value;
            has_value = false;
            input_found = true;
        }
        else if (!has_map && !has_value)
        {
            BWARN("Input '%s' specified neither a value nor a map. A default value will be used", input_name);
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
            out_map->map_name = bname_create(input_name);
            out_map->map = map;
            // Optional property, so it doesn't matter if we get it or not
            out_map->sampler_name = INVALID_BNAME;
            bson_object_property_value_get_bname(&input, "sampler", &out_map->sampler_name);
        }
    }
    else
    {
        // If nothing is specified, use the default for this
        *out_value = default_value;
    }

    return input_found;
}

static b8 extract_input_map_channel_or_vec3(const bson_object* inputs_obj, const char* input_name, basset_material_texture_map map, basset_material_texture* out_map, vec3* out_value, vec3 default_value)
{
    bson_object input = {0};
    b8 input_found = false;
    if (bson_object_property_value_get_object(inputs_obj, input_name, &input))
    {
        b8 has_map = bson_object_property_value_get_bname(&input, "map", &out_map->resource_name);
        b8 has_value = bson_object_property_value_get_vec3(&input, "value", out_value);
        if (has_map && has_value)
        {
            BWARN("Input '%s' specified both a value and a map. The map will be used.", input_name);
            *out_value = default_value;
            has_value = false;
            input_found = true;
        }
        else if (!has_map && !has_value)
        {
            BWARN("Input '%s' specified neither a value nor a map. A default value will be used.", input_name);
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
            out_map->map_name = bname_create(input_name);
            out_map->map = map;
            // Optional property, so it doesn't matter if we get it or not
            out_map->sampler_name = INVALID_BNAME;
            bson_object_property_value_get_bname(&input, "sampler", &out_map->sampler_name);
        }
    }
    else
    {
        // If nothing is specified, use the default for this
        *out_value = default_value;
    }

    return input_found;
}
