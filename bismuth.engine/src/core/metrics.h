#pragma once

#include "defines.h"

BAPI void metrics_initialize(void);
BAPI void metrics_update(f64 frame_elapsed_time);

BAPI f64 metrics_fps(void);
BAPI f64 metrics_frame_time(void);

BAPI void metrics_frame(f64* out_fps, f64* out_frame_ms);
