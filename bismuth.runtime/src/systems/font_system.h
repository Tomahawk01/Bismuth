#pragma once

#include "identifiers/bhandle.h"
#include "bresources/bresource_types.h"
#include "math/math_types.h"
#include "strings/bname.h"

struct font_system_state;

typedef enum font_type
{
    FONT_TYPE_BITMAP,
    FONT_TYPE_SYSTEM
} font_type;

typedef struct system_font_variant
{
    // Handle to the base font
    bhandle base_font;
    // Handle to the font size variant
    bhandle variant;
} system_font_variant;

typedef struct font_system_bitmap_font_config
{
    // The resource name
    bname resource_name;
    // The resource name
    bname package_name;
} font_system_bitmap_font_config;

typedef struct font_system_system_font_config
{
    // The resource name
    bname resource_name;
    // The resource name
    bname package_name;
    // The default font size to be used with the system font
    u16 default_size;
} font_system_system_font_config;

typedef struct font_system_config
{
    /** @brief The max number of bitmap fonts that can be loaded */
    u8 max_bitmap_font_count;
    /** @brief The max number of system fonts that can be loaded */
    u8 max_system_font_count;
    /** @brief The number of bitmap fonts configured in the system */
    u8 bitmap_font_count;
    /** @brief A collection of bitmap fonts configured in the system */
    font_system_bitmap_font_config* bitmap_fonts;
    /** @brief The number of system fonts configured in the system */
    u8 system_font_count;
    /** @brief A collection of system fonts configured in the system */
    font_system_system_font_config* system_fonts;
} font_system_config;

typedef struct font_geometry
{
    /** @brief The number of quads to be drawn */
    u32 quad_count;
    /** @brief The size of the vertex buffer data in bytes */
    u64 vertex_buffer_size;
    /** @brief The size of the index buffer data in bytes */
    u64 index_buffer_size;
    /** @brief The vertex buffer data */
    vertex_2d* vertex_buffer_data;
    /** @brief The index buffer data */
    u32* index_buffer_data;
} font_geometry;

b8 font_system_deserialize_config(const char* config_str, font_system_config* out_config);

b8 font_system_initialize(u64* memory_requirement, void* memory, font_system_config* config);
void font_system_shutdown(struct font_system_state* state);

BAPI b8 font_system_bitmap_font_acquire(struct font_system_state* state, bname name, bhandle* out_font);
BAPI b8 font_system_bitmap_font_load(struct font_system_state* state, bname resource_name, bname package_name);
BAPI b8 font_system_bitmap_font_measure_string(struct font_system_state* state, bhandle font, const char* text, vec2* out_size);
BAPI bresource_texture* font_system_bitmap_font_atlas_get(struct font_system_state* state, bhandle font);
BAPI f32 font_system_bitmap_font_line_height_get(struct font_system_state* state, bhandle font);
BAPI b8 font_system_bitmap_font_generate_geometry(struct font_system_state* state, bhandle font, const char* text, font_geometry* out_geometry);

BAPI b8 font_system_system_font_acquire(struct font_system_state* state, bname name, u16 font_size, system_font_variant* out_variant);
BAPI b8 font_system_system_font_load(struct font_system_state* state, bname resource_name, bname package_name, u16 default_size);
BAPI b8 font_system_system_font_verify_atlas(struct font_system_state* state, system_font_variant variant, const char* text);
BAPI b8 font_system_system_font_measure_string(struct font_system_state* state, system_font_variant variant, const char* text, vec2* out_size);
BAPI f32 font_system_system_font_line_height_get(struct font_system_state* state, system_font_variant variant);
BAPI b8 font_system_system_font_generate_geometry(struct font_system_state* state, system_font_variant variant, const char* text, font_geometry* out_geometry);
BAPI bresource_texture* font_system_system_font_atlas_get(struct font_system_state* state, system_font_variant variant);
