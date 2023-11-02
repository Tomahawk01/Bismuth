#pragma once

#include "defines.h"

BAPI void metrics_initialize();
BAPI void metrics_update(f64 frame_elapsed_time);

BAPI f64 metrics_fps();
BAPI f64 metrics_frame_time();

BAPI void metrics_frame(f64* out_fps, f64* out_frame_ms);
