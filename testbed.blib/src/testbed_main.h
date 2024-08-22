#pragma once

#include <defines.h>

struct application;
struct frame_data;
struct bwindow;

BAPI u64 application_state_size(void);

BAPI b8 application_boot(struct application* game_inst);
BAPI b8 application_initialize(struct application* game_inst);
BAPI b8 application_update(struct application* game_inst, struct frame_data* p_frame_data);

BAPI b8 application_prepare_frame(struct application* app_inst, struct frame_data* p_frame_data);
BAPI b8 application_render_frame(struct application* game_inst, struct frame_data* p_frame_data);

BAPI void application_on_window_resize(struct application* game_inst, const struct bwindow* window);

BAPI void application_shutdown(struct application* game_inst);

BAPI void application_lib_on_unload(struct application* game_inst);
BAPI void application_lib_on_load(struct application* game_inst);
