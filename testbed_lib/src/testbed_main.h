#pragma once

#include <defines.h>

struct application;
struct render_packet;

BAPI u64 application_state_size();

BAPI b8 application_boot(struct application* game_inst);

BAPI b8 application_initialize(struct application* game_inst);

BAPI b8 application_update(struct application* game_inst, f32 delta_time);

BAPI b8 application_render(struct application* game_inst, struct render_packet* packet, f32 delta_time);

BAPI void application_on_resize(struct application* game_inst, u32 width, u32 height);

BAPI void application_shutdown(struct application* game_inst);
