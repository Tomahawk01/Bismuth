#pragma once

#include "defines.h"

#include "audio/audio_types.h"
#include "identifiers/bhandle.h"
#include "platform/vfs.h"

struct application;
struct frame_data;
struct platform_state;
struct console_state;
struct bvar_state;
struct event_state;
struct input_state;
struct timeline_state;
struct resource_state;
struct shader_system_state;
struct renderer_system_state;
struct job_system_state;
struct audio_system_state;
struct xform_system_state;
struct texture_system_state;
struct font_system_state;
struct material_system_state;
struct geometry_system_state;
struct light_system_state;
struct camera_system_state;
struct plugin_system_state;
struct rendergraph_system_state;
struct asset_system_state;
struct vfs_state;
struct bwindow;

typedef struct engine_system_states
{
    u64 platform_memory_requirement;
    struct platform_state* platform_system;

    u64 console_memory_requirement;
    struct console_state* console_system;

    u64 bvar_system_memory_requirement;
    struct bvar_state* bvar_system;

    u64 event_system_memory_requirement;
    struct event_state* event_system;

    u64 input_system_memory_requirement;
    struct input_state* input_system;

    u64 timeline_system_memory_requirement;
    struct timeline_system_state* timeline_system;

    u64 resource_system_memory_requirement;
    struct resource_state* resource_system;

    u64 shader_system_memory_requirement;
    struct shader_system_state* shader_system;

    u64 renderer_system_memory_requirement;
    struct renderer_system_state* renderer_system;

    u64 job_system_memory_requirement;
    struct job_system_state* job_system;

    u64 audio_system_memory_requirement;
    struct audio_system_state* audio_system;

    u64 xform_system_memory_requirement;
    struct xform_system_state* xform_system;

    u64 texture_system_memory_requirement;
    struct texture_system_state* texture_system;

    u64 font_system_memory_requirement;
    struct font_system_state* font_system;

    u64 material_system_memory_requirement;
    struct material_system_state* material_system;

    u64 geometry_system_memory_requirement;
    struct geometry_system_state* geometry_system;

    u64 light_system_memory_requirement;
    struct light_system_state* light_system;

    u64 camera_system_memory_requirement;
    struct camera_system_state* camera_system;

    u64 plugin_system_memory_requirement;
    struct plugin_system_state* plugin_system;

    u64 rendergraph_system_memory_requirement;
    struct rendergraph_system_state* rendergraph_system;

    u64 vfs_system_memory_requirement;
    struct vfs_state* vfs_system_state;

    u64 asset_system_memory_requirement;
    struct asset_system_state* asset_state;

    u64 bresource_system_memory_requirement;
    struct bresource_system_state* bresource_state;
} engine_system_states;

BAPI b8 engine_create(struct application* game_inst);
BAPI b8 engine_run(struct application* game_inst);

void engine_on_event_system_initialized(void);

BAPI const struct frame_data* engine_frame_data_get(void);

BAPI const engine_system_states* engine_systems_get(void);

BAPI b_handle engine_external_system_register(u64 system_state_memory_requirement);

BAPI void* engine_external_system_state_get(b_handle system_handle);

BAPI struct bwindow* engine_active_window_get(void);
