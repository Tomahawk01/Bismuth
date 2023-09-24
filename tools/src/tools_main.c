#include <defines.h>
#include <core/logger.h>
#include <core/bstring.h>

// For executing shell commands
#include <stdlib.h>

void print_help();
i32 process_shaders(i32 argc, char** argv);

i32 main(i32 argc, char** argv)
{
    // First argument is always the program itself
    if (argc < 2)
    {
        BERROR("Bismuth tools requires at least one argument");
        print_help();
        return -1;
    }

    // Second argument tells what mode to go into
    if (strings_equali(argv[1], "buildshaders") || strings_equali(argv[1], "bshaders"))
    {
        return process_shaders(argc, argv);
    }
    else
    {
        BERROR("Unrecognized argument '%s'", argv[1]);
        print_help();
        return -2;
    }

    return 0;
}

i32 process_shaders(i32 argc, char** argv)
{
    if (argc < 3)
    {
        BERROR("Build shaders mode requires at least one additional argument");
        return -3;
    }

    // Starting at third argument. One argument = 1 shader
    for (u32 i = 2; i < argc; ++i)
    {
        char* sdk_path = getenv("VULKAN_SDK");
        if (!sdk_path)
        {
            BERROR("Environment variable VULKAN_SDK not found. Check your Vulkan installation");
            return -4;
        }

        char end_path[10];
        i32 length = string_length(argv[i]);
        string_ncopy(end_path, argv[i] + length - 9, 9);

        // Parse stage from file name
        char stage[5];
        if (strings_equali(end_path, "frag.glsl"))
            string_ncopy(stage, "frag", 4);
        else if (strings_equali(end_path, "vert.glsl"))
            string_ncopy(stage, "vert", 4);
        else if (strings_equali(end_path, "geom.glsl"))
            string_ncopy(stage, "geom", 4);
        else if (strings_equali(end_path, "comp.glsl"))
            string_ncopy(stage, "comp", 4);
        stage[4] = 0;

        // Output filename, just has different extension of spv
        char out_filename[255];
        string_ncopy(out_filename, argv[i], length - 4);
        string_ncopy(out_filename + length - 4, "spv", 3);
        out_filename[length - 1] = 0;

        BINFO("Processing %s -> %s...", argv[i], out_filename);

        // Construct command and execute it
        char command[4096];
        string_format(command, "%s/bin/glslc -fshader-stage=%s %s -o %s", sdk_path, stage, argv[i], out_filename);
        // Vulkan shader compilation
        i32 retcode = system(command);
        if (retcode != 0)
        {
            BERROR("Error compiling shader. See logs. Aborting process...");
            return -5;
        }
    }

    BINFO("Successfully processed all shaders");
    return 0;
}

void print_help()
{
#ifdef BPLATFORM_WINDOWS
    const char* extension = ".exe";
#else
    const char* extension = "";
#endif
    BINFO(
        "Bismuth Game Engine Tools\n\
  usage:  tools%s <mode> [arguments...]\n\
  \n\
  modes:\n\
    buildshaders -  Builds shaders provided in arguments. For example,\n\
                    to compile Vulkan shaders to .spv from GLSL, a list of filenames\n\
                    should be provided that all end in <stage>.glsl, where <stage> is\n\
                    replaced by one of the following supported stages:\n\
                        vert, frag, geom, comp\n\
                    Compiled .spv file is output to the same path as the input file\n",
        extension);
}
