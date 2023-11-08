#include <core/console.h>
#include <core/event.h>
#include <application_types.h>

void game_command_exit(console_command_context context)
{
    BDEBUG("game exit called!");
    event_fire(EVENT_CODE_APPLICATION_QUIT, 0, (event_context){});
}

void game_setup_commands(application* game_inst)
{
    console_register_command("exit", 0, game_command_exit);
    console_register_command("quit", 0, game_command_exit);
}

void game_remove_commands(struct application* game_inst)
{
    console_unregister_command("exit");
    console_unregister_command("quit");
}
