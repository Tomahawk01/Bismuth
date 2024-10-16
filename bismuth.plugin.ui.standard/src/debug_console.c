#include "debug_console.h"

#include <containers/darray.h>
#include <core/console.h>
#include <core/event.h>
#include <core/input.h>
#include <memory/bmemory.h>
#include <resources/font_types.h>
#include <resources/resource_types.h>
#include <strings/bstring.h>

#include "controls/sui_label.h"
#include "controls/sui_panel.h"
#include "controls/sui_textbox.h"
#include "standard_ui_system.h"

static void debug_console_entry_box_on_key(standard_ui_state* state, sui_control* self, sui_keyboard_event evt);

b8 debug_console_consumer_write(void* inst, log_level level, const char* message)
{
    debug_console_state* state = (debug_console_state*)inst;
    if (state)
    {
        // Not necessarily failure, but move on if not loaded
        if (!state->loaded)
            return true;
        // For high-priority error/fatal messages don't bother with splitting, just output them
        if (level <= LOG_LEVEL_ERROR)
        {
            darray_push(state->lines, string_trim(string_duplicate(message)));
            state->dirty = true;
            return true;
        }
        // Create new copy of the string, and try splitting it by new lines to make each one count as new line
        char** split_message = darray_create(char*);
        u32 count = string_split(message, '\n', &split_message, true, false);
        // Push each to array as new line
        for (u32 i = 0; i < count; ++i)
            darray_push(state->lines, split_message[i]);

        darray_destroy(split_message);
        state->dirty = true;
    }
    return true;
}

static b8 debug_console_on_resize(u16 code, void* sender, void* listener_inst, event_context context)
{
    u16 width = context.data.u16[0];
    /* u16 height = context.data.u16[1]; */

    debug_console_state* state = listener_inst;
    vec2 size = sui_panel_size(state->sui_state, &state->bg_panel);
    sui_panel_control_resize(state->sui_state, &state->bg_panel, (vec2){width, size.y});

    return false;
}

void debug_console_create(standard_ui_state* sui_state, debug_console_state* out_console_state)
{
    if (out_console_state)
    {
        out_console_state->line_display_count = 10;
        out_console_state->line_offset = 0;
        out_console_state->lines = darray_create(char*);
        out_console_state->visible = false;
        out_console_state->history = darray_create(command_history_entry);
        out_console_state->history_offset = -1;
        out_console_state->loaded = false;
        out_console_state->sui_state = sui_state;

        console_consumer_register(out_console_state, debug_console_consumer_write, &out_console_state->console_consumer_id);

        // Register for key events
        event_register(EVENT_CODE_WINDOW_RESIZED, out_console_state, debug_console_on_resize);
    }
}

b8 debug_console_load(debug_console_state* state)
{
    if (!state)
    {
        BFATAL("debug_console_load() called before console was initialized!");
        return false;
    }

    u16 font_size = 31;
    f32 height = 50.0f + (font_size * state->line_display_count + 1); // Account for padding and textbox at the bottom

    if (!sui_panel_control_create(state->sui_state, "debug_console_bg_panel", (vec2){1280.0f, height}, (vec4){0.0f, 0.0f, 0.0f, 0.75f}, &state->bg_panel))
    {
        BERROR("Failed to create background panel");
    }
    else
    {
        if (!sui_panel_control_load(state->sui_state, &state->bg_panel))
        {
            BERROR("Failed to load background panel");
        }
        else
        {
            if (!standard_ui_system_register_control(state->sui_state, &state->bg_panel))
            {
                BERROR("Unable to register control");
            }
            else
            {
                if (!standard_ui_system_control_add_child(state->sui_state, 0, &state->bg_panel))
                {
                    BERROR("Failed to parent background panel");
                }
                else
                {
                    state->bg_panel.is_active = true;
                    state->bg_panel.is_visible = false;
                    if (!standard_ui_system_update_active(state->sui_state, &state->bg_panel))
                        BERROR("Unable to update active state");
                }
            }
        }
    }

    // Create a ui text control for rendering
    if (!sui_label_control_create(state->sui_state, "debug_console_log_text", FONT_TYPE_SYSTEM, "Noto Sans CJK JP", font_size, "", &state->text_control))
    {
        BFATAL("Unable to create text control for debug console");
        return false;
    }
    else
    {
        if (!sui_panel_control_load(state->sui_state, &state->text_control))
        {
            BERROR("Failed to load text control");
        }
        else
        {
            if (!standard_ui_system_register_control(state->sui_state, &state->text_control))
            {
                BERROR("Unable to register control");
            }
            else
            {
                if (!standard_ui_system_control_add_child(state->sui_state, &state->bg_panel, &state->text_control))
                {
                    BERROR("Failed to parent background panel");
                }
                else
                {
                    state->text_control.is_active = true;
                    if (!standard_ui_system_update_active(state->sui_state, &state->text_control))
                        BERROR("Unable to update active state");
                }
            }
        }
    }

    sui_control_position_set(state->sui_state, &state->text_control, (vec3){3.0f, font_size, 0.0f});

    // Create another ui text control for rendering typed text
    // new one
    if (!sui_textbox_control_create(state->sui_state, "debug_console_entry_textbox", FONT_TYPE_SYSTEM, "Noto Sans CJK JP", font_size, "", &state->entry_textbox))
    {
        BFATAL("Unable to create entry textbox control for debug console");
        return false;
    }
    else
    {
        if (!state->entry_textbox.load(state->sui_state, &state->entry_textbox))
        {
            BERROR("Failed to load entry textbox for debug console");
        }
        else
        {
            state->entry_textbox.user_data = state;
            state->entry_textbox.user_data_size = sizeof(debug_console_state*);
            state->entry_textbox.on_key = debug_console_entry_box_on_key;
            if (!standard_ui_system_register_control(state->sui_state, &state->entry_textbox))
            {
                BERROR("Unable to register control");
            }
            else
            {
                if (!standard_ui_system_control_add_child(state->sui_state, &state->bg_panel, &state->entry_textbox))
                {
                    BERROR("Failed to parent textbox control to background panel of debug console");
                }
                else
                {
                    state->entry_textbox.is_active = true;
                    if (!standard_ui_system_update_active(state->sui_state, &state->entry_textbox))
                        BERROR("Unable to update active state");
                }
            }
        }
    }

    sui_control_position_set(state->sui_state, &state->entry_textbox, (vec3){3.0f, 10.0f + (font_size * state->line_display_count), 0.0f});
    state->loaded = true;

    return true;
}

void debug_console_unload(debug_console_state* state)
{
    if (state)
        state->loaded = false;
}

void debug_console_update(debug_console_state* state)
{
    if (state && state->loaded && state->dirty)
    {
        u32 line_count = darray_length(state->lines);
        u32 max_lines = BMIN(state->line_display_count, BMAX(line_count, state->line_display_count));

        // Calculate min line first, taking into account line offset as well
        u32 min_line = BMAX(line_count - max_lines - state->line_offset, 0);
        u32 max_line = min_line + max_lines - 1;

        char buffer[16384];
        bzero_memory(buffer, sizeof(char) * 16384);
        u32 buffer_pos = 0;
        for (u32 i = min_line; i <= max_line; ++i)
        {
            // TODO: insert color codes for the message type

            const char* line = state->lines[i];
            u32 line_length = string_length(line);
            for (u32 c = 0; c < line_length; c++, buffer_pos++)
                buffer[buffer_pos] = line[c];

            // Append newline
            buffer[buffer_pos] = '\n';
            buffer_pos++;
        }

        // Make sure string is null-terminated
        buffer[buffer_pos] = '\0';

        // Once string is built, set the text
        sui_label_text_set(state->sui_state, &state->text_control, buffer);

        state->dirty = false;
    }
}

static void debug_console_entry_box_on_key(standard_ui_state* state, sui_control* self, sui_keyboard_event evt)
{
    if (evt.type == SUI_KEYBOARD_EVENT_TYPE_PRESS)
    {
        u16 key_code = evt.key;

        if (key_code == KEY_ENTER)
        {
            debug_console_state* state = self->internal_data;
            const char* entry_control_text = sui_textbox_text_get(state->sui_state, self);
            u32 len = string_length(entry_control_text);
            if (len > 0)
            {
                // Keep command in the history list
                command_history_entry entry = {0};
                entry.command = string_duplicate(entry_control_text);
                if (entry.command)
                {
                    darray_push(((debug_console_state*)self->user_data)->history, entry);

                    // Execute command and clear the text
                    if (!console_command_execute(entry_control_text))
                    {
                        // TODO: handle error
                    }
                }
                // Clear text
                sui_textbox_text_set(state->sui_state, self, "");
            }
        }
    }
}

void debug_console_on_lib_load(debug_console_state* state, b8 update_consumer)
{
    if (update_consumer)
    {
        state->entry_textbox.on_key = debug_console_entry_box_on_key;
        event_register(EVENT_CODE_WINDOW_RESIZED, state, debug_console_on_resize);
        console_consumer_update(state->console_consumer_id, state, debug_console_consumer_write);
    }
}

void debug_console_on_lib_unload(debug_console_state* state)
{
    state->entry_textbox.on_key = 0;
    event_unregister(EVENT_CODE_WINDOW_RESIZED, state, debug_console_on_resize);
    console_consumer_update(state->console_consumer_id, 0, 0);
}

sui_control* debug_console_get_text(debug_console_state* state)
{
    if (state)
        return &state->text_control;
    return 0;
}

sui_control* debug_console_get_entry_text(debug_console_state* state)
{
    if (state)
        return &state->entry_textbox;

    return 0;
}

b8 debug_console_visible(debug_console_state* state)
{
    if (!state)
        return false;

    return state->visible;
}

void debug_console_visible_set(debug_console_state* state, b8 visible)
{
    if (state)
    {
        state->visible = visible;
        state->bg_panel.is_visible = visible;
        standard_ui_system_focus_control(state->sui_state, visible ? &state->entry_textbox : 0);
        input_key_repeats_enable(visible);
    }
}

void debug_console_move_up(debug_console_state* state)
{
    if (state)
    {
        state->dirty = true;
        u32 line_count = darray_length(state->lines);
        if (line_count <= state->line_display_count)
        {
            state->line_offset = 0;
            return;
        }
        state->line_offset++;
        state->line_offset = BMIN(state->line_offset, line_count - state->line_display_count);
    }
}

void debug_console_move_down(debug_console_state* state)
{
    if (state)
    {
        if (state->line_offset == 0)
            return;

        state->dirty = true;
        u32 line_count = darray_length(state->lines);
        if (line_count <= state->line_display_count)
        {
            state->line_offset = 0;
            return;
        }

        state->line_offset--;
        state->line_offset = BMAX(state->line_offset, 0);
    }
}

void debug_console_move_to_top(debug_console_state* state)
{
    if (state)
    {
        state->dirty = true;
        u32 line_count = darray_length(state->lines);
        if (line_count <= state->line_display_count)
        {
            state->line_offset = 0;
            return;
        }

        state->line_offset = line_count - state->line_display_count;
    }
}

void debug_console_move_to_bottom(debug_console_state* state)
{
    if (state)
    {
        state->dirty = true;
        state->line_offset = 0;
    }
}

void debug_console_history_back(debug_console_state* state)
{
    if (state)
    {
        i32 length = darray_length(state->history);
        if (length > 0)
        {
            state->history_offset = BMIN(state->history_offset++, length - 1);
            i32 idx = length - state->history_offset - 1;
            sui_textbox_text_set(state->sui_state, &state->entry_textbox, state->history[idx].command);
        }
    }
}

void debug_console_history_forward(debug_console_state* state)
{
    if (state)
    {
        i32 length = darray_length(state->history);
        if (length > 0)
        {
            state->history_offset = BMAX(state->history_offset - 1, -1);
            i32 idx = length - state->history_offset - 1;
            sui_textbox_text_set(state->sui_state, &state->entry_textbox, state->history_offset == -1 ? "" : state->history[idx].command);
        }
    }
}
