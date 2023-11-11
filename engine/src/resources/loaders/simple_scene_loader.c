#include "simple_scene_loader.h"

#include "core/logger.h"
#include "core/bmemory.h"
#include "core/bstring.h"
#include "resources/resource_types.h"
#include "systems/resource_system.h"
#include "math/bmath.h"
#include "math/transform.h"
#include "loader_utils.h"
#include "containers/darray.h"
#include "platform/filesystem.h"

typedef enum simple_scene_parse_mode
{
    SIMPLE_SCENE_PARSE_MODE_ROOT,
    SIMPLE_SCENE_PARSE_MODE_SCENE,
    SIMPLE_SCENE_PARSE_MODE_SKYBOX,
    SIMPLE_SCENE_PARSE_MODE_DIRECTIONAL_LIGHT,
    SIMPLE_SCENE_PARSE_MODE_POINT_LIGHT,
    SIMPLE_SCENE_PARSE_MODE_MESH,
    SIMPLE_SCENE_PARSE_MODE_TERRAIN
} simple_scene_parse_mode;

static b8 try_change_mode(const char* value, simple_scene_parse_mode* current, simple_scene_parse_mode expected_current, simple_scene_parse_mode target);

static b8 simple_scene_loader_load(struct resource_loader* self, const char* name, void* params, resource* out_resource)
{
    if (!self || !name || !out_resource)
        return false;

    char* format_str = "%s/%s/%s%s";
    char full_file_path[512];
    string_format(full_file_path, format_str, resource_system_base_path(), self->type_path, name, ".bss");

    file_handle f;
    if (!filesystem_open(full_file_path, FILE_MODE_READ, false, &f))
    {
        BERROR("simple_scene_loader_load - unable to open simple scene file for reading: '%s'", full_file_path);
        return false;
    }

    out_resource->full_path = string_duplicate(full_file_path);

    simple_scene_config* resource_data = ballocate(sizeof(simple_scene_config), MEMORY_TAG_RESOURCE);
    bzero_memory(resource_data, sizeof(simple_scene_config));

    // Set some defaults, create arrays.
    resource_data->description = 0;
    resource_data->name = string_duplicate(name);
    resource_data->point_lights = darray_create(point_light_simple_scene_config);
    resource_data->meshes = darray_create(mesh_simple_scene_config);
    resource_data->terrains = darray_create(terrain_simple_scene_config);

    u32 version = 0;
    simple_scene_parse_mode mode = SIMPLE_SCENE_PARSE_MODE_ROOT;

    point_light_simple_scene_config current_point_light_config = {0};
    mesh_simple_scene_config current_mesh_config = {0};
    terrain_simple_scene_config current_terrain_config = {0};

    // Read each line of the file
    char line_buf[512] = "";
    char* p = &line_buf[0];
    u64 line_length = 0;
    u32 line_number = 1;
    while (filesystem_read_line(&f, 511, &p, &line_length))
    {
        char* trimmed = string_trim(line_buf);

        // Get trimmed length
        line_length = string_length(trimmed);

        // Skip blank lines and comments
        if (line_length < 1 || trimmed[0] == '#')
        {
            line_number++;
            continue;
        }

        if (trimmed[0] == '[')
        {
            if (version == 0)
            {
                BERROR("Error loading simple scene file, !version was not set before attempting to change modes");
                return false;
            }

            // Change modes
            if (strings_equali(trimmed, "[Scene]"))
            {
                if (!try_change_mode(trimmed, &mode, SIMPLE_SCENE_PARSE_MODE_ROOT, SIMPLE_SCENE_PARSE_MODE_SCENE))
                    return false;
            }
            else if (strings_equali(trimmed, "[/Scene]"))
            {
                if (!try_change_mode(trimmed, &mode, SIMPLE_SCENE_PARSE_MODE_SCENE, SIMPLE_SCENE_PARSE_MODE_ROOT))
                    return false;
            }
            else if (strings_equali(trimmed, "[Skybox]"))
            {
                if (!try_change_mode(trimmed, &mode, SIMPLE_SCENE_PARSE_MODE_ROOT, SIMPLE_SCENE_PARSE_MODE_SKYBOX))
                    return false;
            }
            else if (strings_equali(trimmed, "[/Skybox]"))
            {
                if (!try_change_mode(trimmed, &mode, SIMPLE_SCENE_PARSE_MODE_SKYBOX, SIMPLE_SCENE_PARSE_MODE_ROOT))
                    return false;
            }
            else if (strings_equali(trimmed, "[DirectionalLight]"))
            {
                if (!try_change_mode(trimmed, &mode, SIMPLE_SCENE_PARSE_MODE_ROOT, SIMPLE_SCENE_PARSE_MODE_DIRECTIONAL_LIGHT))
                    return false;
            }
            else if (strings_equali(trimmed, "[/DirectionalLight]"))
            {
                if (!try_change_mode(trimmed, &mode, SIMPLE_SCENE_PARSE_MODE_DIRECTIONAL_LIGHT, SIMPLE_SCENE_PARSE_MODE_ROOT))
                    return false;
            }
            else if (strings_equali(trimmed, "[PointLight]"))
            {
                if (!try_change_mode(trimmed, &mode, SIMPLE_SCENE_PARSE_MODE_ROOT, SIMPLE_SCENE_PARSE_MODE_POINT_LIGHT))
                    return false;
                bzero_memory(&current_point_light_config, sizeof(point_light_simple_scene_config));
            }
            else if (strings_equali(trimmed, "[/PointLight]"))
            {
                if (!try_change_mode(trimmed, &mode, SIMPLE_SCENE_PARSE_MODE_POINT_LIGHT, SIMPLE_SCENE_PARSE_MODE_ROOT))
                    return false;
                // Push into the array, then cleanup
                darray_push(resource_data->point_lights, current_point_light_config);
                bzero_memory(&current_point_light_config, sizeof(point_light_simple_scene_config));
            }
            else if (strings_equali(trimmed, "[Mesh]"))
            {
                if (!try_change_mode(trimmed, &mode, SIMPLE_SCENE_PARSE_MODE_ROOT, SIMPLE_SCENE_PARSE_MODE_MESH))
                    return false;
                bzero_memory(&current_mesh_config, sizeof(mesh_simple_scene_config));
                // Also setup default transform
                current_mesh_config.transform = transform_create();
            }
            else if (strings_equali(trimmed, "[/Mesh]"))
            {
                if (!try_change_mode(trimmed, &mode, SIMPLE_SCENE_PARSE_MODE_MESH, SIMPLE_SCENE_PARSE_MODE_ROOT))
                    return false;
                if (!current_mesh_config.name || !current_mesh_config.resource_name)
                {
                    BWARN("Format error: meshes require both name and resource name. Mesh not added");
                    continue;
                }
                // Push into array, then cleanup
                darray_push(resource_data->meshes, current_mesh_config);
                bzero_memory(&current_mesh_config, sizeof(mesh_simple_scene_config));
            }
            else if (strings_equali(trimmed, "[Terrain]"))
            {
                if (!try_change_mode(trimmed, &mode, SIMPLE_SCENE_PARSE_MODE_ROOT, SIMPLE_SCENE_PARSE_MODE_TERRAIN))
                    return false;
                bzero_memory(&current_terrain_config, sizeof(terrain_simple_scene_config));
                // Also setup default transform
                current_terrain_config.xform = transform_create();
            }
            else if (strings_equali(trimmed, "[/Terrain]"))
            {
                if (!try_change_mode(trimmed, &mode, SIMPLE_SCENE_PARSE_MODE_TERRAIN, SIMPLE_SCENE_PARSE_MODE_ROOT))
                    return false;
                if (!current_terrain_config.name || !current_terrain_config.resource_name)
                {
                    BWARN("Format error: Terrains require both name and resource name. Terrain not added");
                    continue;
                }
                // Push into the array, then cleanup
                darray_push(resource_data->terrains, current_terrain_config);
                bzero_memory(&current_mesh_config, sizeof(terrain_simple_scene_config));
            }
            else
            {
                BERROR("Error loading simple scene file: format error. Unexpected object type '%s'", trimmed);
                return false;
            }
        }
        else
        {
            // Split into var/value
            i32 equal_index = string_index_of(trimmed, '=');
            if (equal_index == -1)
            {
                BWARN("Potential formatting issue found in file '%s': '=' token not found. Skipping line %ui", full_file_path, line_number);
                line_number++;
                continue;
            }

            // Assume a max of 64 characters for variable name
            char raw_var_name[64];
            bzero_memory(raw_var_name, sizeof(char) * 64);
            string_mid(raw_var_name, trimmed, 0, equal_index);
            char* trimmed_var_name = string_trim(raw_var_name);

            // Assume a max of 511-65 (446) for max length of the value to account for the variable name and the '='
            char raw_value[446];
            bzero_memory(raw_value, sizeof(char) * 446);
            string_mid(raw_value, trimmed, equal_index + 1, -1);  // Read rest of the line
            char* trimmed_value = string_trim(raw_value);

            // Process variable
            if (strings_equali(trimmed_var_name, "!version"))
            {
                if (mode != SIMPLE_SCENE_PARSE_MODE_ROOT)
                {
                    BERROR("Attempting to set version inside of non-root mode");
                    return false;
                }
                if (!string_to_u32(trimmed_value, &version))
                {
                    BERROR("Invalid value for version: %s", trimmed_value);
                    // TODO: cleanup config
                    return false;
                }
            }
            else if (strings_equali(trimmed_var_name, "name"))
            {
                switch (mode)
                {
                    default:
                    case SIMPLE_SCENE_PARSE_MODE_ROOT:
                        BWARN("Format warning: Cannot process name in root node");
                        break;
                    case SIMPLE_SCENE_PARSE_MODE_SCENE:
                        resource_data->name = string_duplicate(trimmed_value);
                        break;
                    case SIMPLE_SCENE_PARSE_MODE_DIRECTIONAL_LIGHT:
                        resource_data->directional_light_config.name = string_duplicate(trimmed_value);
                        break;
                    case SIMPLE_SCENE_PARSE_MODE_SKYBOX:
                        resource_data->skybox_config.name = string_duplicate(trimmed_value);
                        break;
                    case SIMPLE_SCENE_PARSE_MODE_POINT_LIGHT:
                        current_point_light_config.name = string_duplicate(trimmed_value);
                        break;
                    case SIMPLE_SCENE_PARSE_MODE_MESH:
                        current_mesh_config.name = string_duplicate(trimmed_value);
                        break;
                    case SIMPLE_SCENE_PARSE_MODE_TERRAIN:
                        current_terrain_config.name = string_duplicate(trimmed_value);
                        break;
                }
            }
            else if (strings_equali(trimmed_var_name, "color"))
            {
                switch (mode)
                {
                    default:
                        BWARN("Format warning: Cannot process name in the current node");
                        break;
                    case SIMPLE_SCENE_PARSE_MODE_DIRECTIONAL_LIGHT:
                        if (!string_to_vec4(trimmed_value, &resource_data->directional_light_config.color))
                        {
                            BWARN("Format error parsing color. Default value used");
                            resource_data->directional_light_config.color = vec4_one();
                        }
                        break;
                    case SIMPLE_SCENE_PARSE_MODE_POINT_LIGHT:
                        if (!string_to_vec4(trimmed_value, &current_point_light_config.color))
                        {
                            BWARN("Format error parsing color. Default value used");
                            current_point_light_config.color = vec4_one();
                        }
                        break;
                }
            }
            else if (strings_equali(trimmed_var_name, "description"))
            {
                if (mode == SIMPLE_SCENE_PARSE_MODE_SCENE)
                    resource_data->description = string_duplicate(trimmed_value);
                else
                    BWARN("Format warning: Cannot process description in the current node");
            }
            else if (strings_equali(trimmed_var_name, "cubemap_name"))
            {
                if (mode == SIMPLE_SCENE_PARSE_MODE_SKYBOX)
                    resource_data->skybox_config.cubemap_name = string_duplicate(trimmed_value);
                else
                    BWARN("Format warning: Cannot process cubemap_name in the current node");
            }
            else if (strings_equali(trimmed_var_name, "resource_name"))
            {
                if (mode == SIMPLE_SCENE_PARSE_MODE_MESH)
                    current_mesh_config.resource_name = string_duplicate(trimmed_value);
                else if(mode == SIMPLE_SCENE_PARSE_MODE_TERRAIN)
                    current_terrain_config.resource_name = string_duplicate(trimmed_value);
                else
                    BWARN("Format warning: Cannot process resource_name in the current node");
            }
            else if (strings_equali(trimmed_var_name, "parent"))
            {
                if (mode == SIMPLE_SCENE_PARSE_MODE_MESH)
                    current_mesh_config.parent_name = string_duplicate(trimmed_value);
                else
                    BWARN("Format warning: Cannot process resource_name in the current node");
            }
            else if (strings_equali(trimmed_var_name, "direction"))
            {
                if (mode == SIMPLE_SCENE_PARSE_MODE_DIRECTIONAL_LIGHT)
                {
                    if (!string_to_vec4(trimmed_value, &resource_data->directional_light_config.direction))
                    {
                        BWARN("Error parsing directional light direction as vec4. Using default value");
                        resource_data->directional_light_config.direction = (vec4){-0.57735f, -0.57735f, -0.57735f, 0.0f};
                    }
                }
                else
                {
                    BWARN("Format warning: Cannot process direction in the current node");
                }
            }
            else if (strings_equali(trimmed_var_name, "position"))
            {
                if (mode == SIMPLE_SCENE_PARSE_MODE_POINT_LIGHT)
                {
                    if (!string_to_vec4(trimmed_value, &current_point_light_config.position))
                    {
                        BWARN("Error parsing point light position as vec4. Using default value");
                        current_point_light_config.position = vec4_zero();
                    }
                }
                else
                {
                    BWARN("Format warning: Cannot process direction in the current node");
                }
            }
            else if (strings_equali(trimmed_var_name, "transform"))
            {
                if (mode == SIMPLE_SCENE_PARSE_MODE_MESH)
                {
                    if (!string_to_transform(trimmed_value, &current_mesh_config.transform))
                        BWARN("Error parsing mesh transform. Using default value");
                }
                else if (mode == SIMPLE_SCENE_PARSE_MODE_TERRAIN)
                {
                    if (!string_to_transform(trimmed_value, &current_terrain_config.xform))
                        BWARN("Error parsing terrain transform. Using default value");
                }
                else
                {
                    BWARN("Format warning: Cannot process transform in the current node");
                }
            }
            else if (strings_equali(trimmed_var_name, "constant_f"))
            {
                if (mode == SIMPLE_SCENE_PARSE_MODE_POINT_LIGHT)
                {
                    if (!string_to_f32(trimmed_value, &current_point_light_config.constant_f))
                    {
                        BWARN("Error parsing point light constant_f. Using default value");
                        current_point_light_config.constant_f = 1.0f;
                    }
                }
                else
                {
                    BWARN("Format warning: Cannot process constant in the current node");
                }
            }
            else if (strings_equali(trimmed_var_name, "linear"))
            {
                if (mode == SIMPLE_SCENE_PARSE_MODE_POINT_LIGHT)
                {
                    if (!string_to_f32(trimmed_value, &current_point_light_config.linear))
                    {
                        BWARN("Error parsing point light linear. Using default value");
                        current_point_light_config.linear = 0.35f;
                    }
                }
                else
                {
                    BWARN("Format warning: Cannot process linear in the current node");
                }
            }
            else if (strings_equali(trimmed_var_name, "quadratic"))
            {
                if (mode == SIMPLE_SCENE_PARSE_MODE_POINT_LIGHT)
                {
                    if (!string_to_f32(trimmed_value, &current_point_light_config.quadratic))
                    {
                        BWARN("Error parsing point light quadratic. Using default value");
                        current_point_light_config.quadratic = 0.44f;
                    }
                }
                else
                {
                    BWARN("Format warning: Cannot process quadratic in the current node");
                }
            }
        }

        // TODO: add more fields

        // Clear line buffer
        bzero_memory(line_buf, sizeof(char) * 512);
        line_number++;
    }

    filesystem_close(&f);

    out_resource->data = resource_data;
    out_resource->data_size = sizeof(simple_scene_config);

    return true;
}

static void simple_scene_loader_unload(struct resource_loader* self, resource* resource)
{
    simple_scene_config* data = (simple_scene_config*)resource->data;

    if (data->meshes)
    {
        u32 length = darray_length(data->meshes);
        for (u32 i = 0; i < length; ++i)
        {
            if (data->meshes[i].name)
                bfree(data->meshes[i].name, string_length(data->meshes[i].name) + 1, MEMORY_TAG_STRING);

            if (data->meshes[i].parent_name)
                bfree(data->meshes[i].parent_name, string_length(data->meshes[i].parent_name) + 1, MEMORY_TAG_STRING);

            if (data->meshes[i].resource_name)
                bfree(data->meshes[i].resource_name, string_length(data->meshes[i].resource_name) + 1, MEMORY_TAG_STRING);
        }
        darray_destroy(data->meshes);
    }
    if (data->point_lights)
    {
        u32 length = darray_length(data->point_lights);
        for (u32 i = 0; i < length; ++i)
        {
            if (data->point_lights[i].name)
                bfree(data->point_lights[i].name, string_length(data->point_lights[i].name) + 1, MEMORY_TAG_STRING);
        }
        darray_destroy(data->point_lights);
    }

    if (data->directional_light_config.name)
        bfree(data->directional_light_config.name, string_length(data->directional_light_config.name) + 1, MEMORY_TAG_STRING);

    if (data->skybox_config.name)
        bfree(data->skybox_config.name, string_length(data->skybox_config.name) + 1, MEMORY_TAG_STRING);
    if (data->skybox_config.cubemap_name)
        bfree(data->skybox_config.cubemap_name, string_length(data->skybox_config.cubemap_name) + 1, MEMORY_TAG_STRING);

    if (data->name)
        bfree(data->name, string_length(data->name) + 1, MEMORY_TAG_STRING);

    if (data->description)
        bfree(data->description, string_length(data->description) + 1, MEMORY_TAG_STRING);

    if (!resource_unload(self, resource, MEMORY_TAG_RESOURCE))
        BWARN("simple_scene_loader_unload called with nullptr for self or resource");
}

resource_loader simple_scene_resource_loader_create(void)
{
    resource_loader loader;
    loader.type = RESOURCE_TYPE_SIMPLE_SCENE;
    loader.custom_type = 0;
    loader.load = simple_scene_loader_load;
    loader.unload = simple_scene_loader_unload;
    loader.type_path = "scenes";

    return loader;
}

static b8 try_change_mode(const char* value, simple_scene_parse_mode* current, simple_scene_parse_mode expected_current, simple_scene_parse_mode target)
{
    if (*current != expected_current)
    {
        BERROR("Error loading simple scene: format error. Unexpected token '%'", value);
        return false;
    }
    else
    {
        *current = target;
        return true;
    }
}
