#pragma once

#include "vulkan_types.h"

void vulkan_image_create(
    vulkan_context* context,
    texture_type type,
    u32 width,
    u32 height,
    u16 layer_count,
    VkFormat format,
    VkImageTiling tiling,
    VkImageUsageFlags usage,
    VkMemoryPropertyFlags memory_flags,
    b32 create_view,
    VkImageAspectFlags view_aspect_flags,
    const char* name,
    u32 mip_levels,
    vulkan_image* out_image);

void vulkan_image_view_create(
    vulkan_context* context,
    texture_type type,
    u16 layer_count,
    i32 layer_index,
    VkFormat format,
    vulkan_image* image,
    VkImageAspectFlags aspect_flags,
    VkImageView* out_view);

// Transitions provided image from old_layout to new_layout
void vulkan_image_transition_layout(
    vulkan_context* context,
    texture_type type,
    vulkan_command_buffer* command_buffer,
    vulkan_image* image,
    VkFormat format,
    VkImageLayout old_layout,
    VkImageLayout new_layout);

b8 vulkan_image_mipmaps_generate(
    vulkan_context* context,
    vulkan_image* image,
    vulkan_command_buffer* command_buffer);

/**
 * Copies data in buffer to provided image.
 * @param context The Vulkan context.
 * @param image The image to copy the buffer's data to.
 * @param buffer The buffer whose data will be copied.
 * @param offset The offset in bytes from the beginning of the buffer.
 * @param command_buffer A pointer to the command buffer to be used for this operation.
 */
void vulkan_image_copy_from_buffer(
    vulkan_context* context,
    texture_type type,
    vulkan_image* image,
    VkBuffer buffer,
    u64 offset,
    vulkan_command_buffer* command_buffer);

void vulkan_image_copy_to_buffer(
    vulkan_context* context,
    texture_type type,
    vulkan_image* image,
    VkBuffer buffer,
    vulkan_command_buffer* command_buffer);

void vulkan_image_copy_pixel_to_buffer(
    vulkan_context* context,
    texture_type type,
    vulkan_image* image,
    VkBuffer buffer,
    u32 x,
    u32 y,
    vulkan_command_buffer* command_buffer);

void vulkan_image_destroy(vulkan_context* context, vulkan_image* image);
