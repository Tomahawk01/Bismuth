#pragma once

#include "renderer/renderer_types.inl"

typedef struct texture_system_config
{
    u32 max_texture_count;
} texture_system_config;

#define DEFAULT_TEXTURE_NAME "default"
#define DEFAULT_SPECULAR_TEXTURE_NAME "default_spec"
#define DEFAULT_NORMAL_TEXTURE_NAME "default_norm"

b8 texture_system_initialize(u64* memory_requirement, void* state, texture_system_config config);
void texture_system_shutdown(void* state);

texture* texture_system_acquire(const char* name, b8 auto_release);
void texture_system_release(const char* name);

texture* texture_system_get_default_texture();
texture* texture_system_get_default_specular_texture();
texture* texture_system_get_default_normal_texture();
