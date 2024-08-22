#include "standard_ui_system.h"

#include <defines.h>

struct bruntime_plugin;
struct frame_data;
struct bwindow;
struct standard_ui_state;

typedef struct standard_ui_plugin_state
{
    u64 sui_state_memory_requirement;
    struct standard_ui_state* state;
    standard_ui_render_data* render_data;
} standard_ui_plugin_state;

BAPI b8 bplugin_create(struct bruntime_plugin* out_plugin);
BAPI b8 bplugin_initialize(struct bruntime_plugin* plugin);
BAPI void bplugin_destroy(struct bruntime_plugin* plugin);

BAPI b8 bplugin_update(struct bruntime_plugin* plugin, struct frame_data* p_frame_data);
BAPI b8 bplugin_frame_prepare(struct bruntime_plugin* plugin, struct frame_data* p_frame_data);
// NOTE: Actual rendering handled by configured rendergraph node
/* BAPI b8 bplugin_render(struct bruntime_plugin* plugin, struct frame_data* p_frame_data); */

BAPI void bplugin_on_window_resized(void* plugin_state, struct bwindow* window, u16 width, u16 height);
