#pragma once

#include "math/math_types.h"
#include "renderer/renderer_types.h"
#include "resources/font_types.h"
#include "resources/resource_types.h"

typedef struct system_font_config
{
    const char* name;
    u16 default_size;
    const char* resource_name;
} system_font_config;

typedef struct bitmap_font_config
{
    const char* name;
    u16 size;
    const char* resource_name;
} bitmap_font_config;

typedef struct font_system_config
{
    system_font_config default_system_font;
    bitmap_font_config default_bitmap_font;
    b8 auto_release;
} font_system_config;

b8 font_system_deserialize_config(const char* config_str, font_system_config* out_config);

b8 font_system_initialize(u64* memory_requirement, void* memory, font_system_config* config);
void font_system_shutdown(void* memory);

BAPI b8 font_system_system_font_load(system_font_config* config);
BAPI b8 font_system_bitmap_font_load(bitmap_font_config* config);

BAPI font_data* font_system_acquire(const char* font_name, u16 font_size, font_type type);
BAPI b8 font_system_release(const char* font_name);

BAPI b8 font_system_verify_atlas(font_data* font, const char* text);

BAPI vec2 font_system_measure_string(font_data* font, const char* text);
