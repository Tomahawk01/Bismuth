@ECHO OFF
REM Build script for cleaning and/or building everything

SET PLATFORM=%1
SET ACTION=%2
SET TARGET=%3

if "%ACTION%" == "build" (
    SET ACTION=all
    SET ACTION_STR=Building
    SET ACTION_STR_PAST=built
    SET DO_VERSION=yes
) else (
    if "%ACTION%" == "clean" (
        SET ACTION=clean
        SET ACTION_STR=Cleaning
        SET ACTION_STR_PAST=cleaned
        SET DO_VERSION=no
    ) else (
        echo "Unknown action %ACTION%. Aborting" && exit
    )
)

if "%PLATFORM%" == "windows" (
    SET ENGINE_LINK=-luser32
) else (
    if "%PLATFORM%" == "linux" (
        SET ENGINE_LINK=
    ) else (
        if "%PLATFORM%" == "macos" (
            SET ENGINE_LINK=
        ) else (
            echo "Unknown platform %PLATFORM%. Aborting" && exit
        )
    )
)

REM del bin\*.pdb

SET INC_CORE_RT=-Ibismuth.core\src -Ibismuth.runtime\src
SET LNK_CORE_RT=-lbismuth.core -lbismuth.runtime

ECHO "%ACTION_STR% everything on %PLATFORM% (%TARGET%)..."

REM Version Generator
make -j -f "Makefile.executable.mak" %ACTION% TARGET=%TARGET% ASSEMBLY=bismuth.tools.versiongen
IF %ERRORLEVEL% NEQ 0 (echo Error:%ERRORLEVEL% && exit)

REM Engine core lib
make -j -f "Makefile.library.mak" %ACTION% TARGET=%TARGET% ASSEMBLY=bismuth.core VER_MAJOR=0 VER_MINOR=7 DO_VERSION=%DO_VERSION% ADDL_LINK_FLAGS="-lgdi32 %ENGINE_LINK%"
IF %ERRORLEVEL% NEQ 0 (echo Error:%ERRORLEVEL% && exit)

REM Engine runtime lib
make -j -f "Makefile.library.mak" %ACTION% TARGET=%TARGET% ASSEMBLY=bismuth.runtime VER_MAJOR=0 VER_MINOR=1 DO_VERSION=%DO_VERSION% ADDL_INC_FLAGS="%INC_CORE_RT%" ADDL_LINK_FLAGS="-lbismuth.core %ENGINE_LINK%"
IF %ERRORLEVEL% NEQ 0 (echo Error:%ERRORLEVEL% && exit)

REM Vulkan Renderer plugin lib
make -j -f "Makefile.library.mak" %ACTION% TARGET=%TARGET% ASSEMBLY=bismuth.plugin.renderer.vulkan VER_MAJOR=0 VER_MINOR=1 DO_VERSION=no ADDL_INC_FLAGS="%INC_CORE_RT% -I%VULKAN_SDK%\include" ADDL_LINK_FLAGS="%LNK_CORE_RT% -lvulkan-1 -lshaderc_shared -L%VULKAN_SDK%\Lib"
IF %ERRORLEVEL% NEQ 0 (echo Error:%ERRORLEVEL% && exit)

REM OpenAL plugin lib
make -j -f "Makefile.library.mak" %ACTION% TARGET=%TARGET% ASSEMBLY=bismuth.plugin.audio.openal VER_MAJOR=0 VER_MINOR=1 DO_VERSION=no ADDL_INC_FLAGS="%INC_CORE_RT% -I'%programfiles(x86)%\OpenAL 1.1 SDK\include'" ADDL_LINK_FLAGS="%LNK_CORE_RT% -lopenal32 -L'%programfiles(x86)%\OpenAL 1.1 SDK\libs\win64'"
IF %ERRORLEVEL% NEQ 0 (echo Error:%ERRORLEVEL% && exit)

REM Standard UI lib
make -j -f "Makefile.library.mak" %ACTION% TARGET=%TARGET% ASSEMBLY=bismuth.plugin.ui.standard VER_MAJOR=0 VER_MINOR=1 DO_VERSION=no ADDL_INC_FLAGS="%INC_CORE_RT%" ADDL_LINK_FLAGS="%LNK_CORE_RT%"
IF %ERRORLEVEL% NEQ 0 (echo Error:%ERRORLEVEL% && exit)

REM Testbed lib
make -j -f "Makefile.library.mak" %ACTION% TARGET=%TARGET% ASSEMBLY=testbed.blib VER_MAJOR=0 VER_MINOR=1 DO_VERSION=no ADDL_INC_FLAGS="%INC_CORE_RT% -Ibismuth.plugin.ui.standard\src -Ibismuth.plugin.audio.openal\src" ADDL_LINK_FLAGS="%LNK_CORE_RT% -lbismuth.plugin.ui.standard -lbismuth.plugin.audio.openal"
IF %ERRORLEVEL% NEQ 0 (echo Error:%ERRORLEVEL% && exit)

@REM ---------------------------------------------------
@REM Executables
@REM ---------------------------------------------------

REM Testbed
make -j -f "Makefile.executable.mak" %ACTION% TARGET=%TARGET% ASSEMBLY=testbed.bapp ADDL_INC_FLAGS="%INC_CORE_RT%" ADDL_LINK_FLAGS="%LNK_CORE_RT%"
IF %ERRORLEVEL% NEQ 0 (echo Error:%ERRORLEVEL% && exit)

REM Tests
make -j -f "Makefile.executable.mak" %ACTION% TARGET=%TARGET% ASSEMBLY=bismuth.core.tests ADDL_INC_FLAGS="%INC_CORE_RT%" ADDL_LINK_FLAGS="%LNK_CORE_RT%"
IF %ERRORLEVEL% NEQ 0 (echo Error:%ERRORLEVEL% && exit)

REM Tools
make -j -f "Makefile.executable.mak" %ACTION% TARGET=%TARGET% ASSEMBLY=bismuth.tools ADDL_INC_FLAGS="%INC_CORE_RT%" ADDL_LINK_FLAGS="%LNK_CORE_RT%"
IF %ERRORLEVEL% NEQ 0 (echo Error:%ERRORLEVEL% && exit)

ECHO All assemblies %ACTION_STR_PAST% successfully on %PLATFORM% (%TARGET%)
