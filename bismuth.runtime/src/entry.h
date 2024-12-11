#pragma once

#include "application/application_config.h"
#include "application/application_types.h"
#include "core/engine.h"
#include "logger.h"
#include "platform/filesystem.h"

// Externally-defined function to create application
extern b8 create_application(application* out_app);

extern b8 initialize_application(application* app);

/**
 * @brief The main entry point of the application
 */
int main(void)
{
    // TODO: load up application config file, get it parsed and ready to hand off
    // Request application instance from the application
    application app_inst = {0};

    const char* app_file_content = filesystem_read_entire_text_file("../testbed.bapp/app_config.bson");
    if (!app_file_content)
    {
        BFATAL("Failed to read app_config.bson file text. Application cannot start");
        return -68;
    }

    if (!application_config_parse_file_content(app_file_content, &app_inst.app_config))
    {
        BFATAL("Failed to parse application config. Cannot start");
        return -69;
    }

    if (!create_application(&app_inst))
    {
        BFATAL("Could not create application!");
        return -1;
    }

    // Ensure the function pointers exist
    if (!(app_inst.render_frame || !app_inst.prepare_frame || app_inst.update || app_inst.initialize))
    {
        BFATAL("The game's function pointers must be assigned!");
        return -2;
    }

    // Initialization
    if (!engine_create(&app_inst))
    {
        BFATAL("Engine failed to create!");
        return 1;
    }

    if (!initialize_application(&app_inst))
    {
        BFATAL("Could not initialize application");
        return -1;
    }

    // Begin engine loop
    if (!engine_run(&app_inst))
    {
        BINFO("Application did not shutdown correctly");
        return 2;
    }

    return 0;
}
