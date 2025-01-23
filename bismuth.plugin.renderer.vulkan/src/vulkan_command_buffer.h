#pragma once

#include "vulkan_types.h"

void vulkan_command_buffer_allocate(
    vulkan_context* context,
    VkCommandPool pool,
    b8 is_primary,
    const char* name,
    vulkan_command_buffer* out_command_buffer,
    u32 secondary_buffer_count);

void vulkan_command_buffer_free(
    vulkan_context* context,
    VkCommandPool pool,
    vulkan_command_buffer* command_buffer);

void vulkan_command_buffer_begin(
    vulkan_command_buffer* command_buffer,
    b8 is_single_use,
    b8 is_renderpass_continue,
    b8 is_simultaneous_use);

void vulkan_command_buffer_end(vulkan_command_buffer* command_buffer);

b8 vulkan_command_buffer_submit(
    vulkan_command_buffer* command_buffer,
    VkQueue queue,
    u32 signal_semaphore_count,
    VkSemaphore* signal_semaphores,
    u32 wait_semaphore_count,
    VkSemaphore* wait_semaphores,
    VkFence fence);

void vulkan_command_buffer_execute_secondary(vulkan_command_buffer* secondary);

void vulkan_command_buffer_reset(vulkan_command_buffer* command_buffer);

/**
 * Allocates and begins recording to out_command_buffer
 */
void vulkan_command_buffer_allocate_and_begin_single_use(
    vulkan_context* context,
    VkCommandPool pool,
    vulkan_command_buffer* out_command_buffer);

/**
 * Ends recording, submits to and waits for queue operation and frees the provided command buffer
 */
void vulkan_command_buffer_end_single_use(
    vulkan_context* context,
    VkCommandPool pool,
    vulkan_command_buffer* command_buffer,
    VkQueue queue);
