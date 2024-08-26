#include <defines.h>

struct bruntime_plugin;

BAPI b8 bplugin_create(struct bruntime_plugin* out_plugin);
BAPI b8 bplugin_initialize(struct bruntime_plugin* plugin);
BAPI void bplugin_destroy(struct bruntime_plugin* plugin);
