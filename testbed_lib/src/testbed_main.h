#pragma once

#include <defines.h>

struct application;
struct render_packet;
struct frame_data;

BAPI u64 application_state_size(void);

BAPI b8 application_boot(struct application* game_inst);

BAPI b8 application_initialize(struct application* game_inst);

BAPI b8 application_update(struct application* game_inst, struct frame_data* p_frame_data);

BAPI b8 application_prepare_render_packet(struct application* app_inst, struct render_packet* packet, struct frame_data* p_frame_data);

BAPI b8 application_render(struct application* game_inst, struct render_packet* packet, struct frame_data* p_frame_data);

BAPI void application_on_resize(struct application* game_inst, u32 width, u32 height);

BAPI void application_shutdown(struct application* game_inst);

BAPI void application_lib_on_unload(struct application* game_inst);
BAPI void application_lib_on_load(struct application* game_inst);
