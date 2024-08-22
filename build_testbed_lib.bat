REM Builds only the testbed library

SET INC_CORE_RT=-Ibismuth.core\src -Ibismuth.runtime\src
SET LNK_CORE_RT=-lbismuth.core -lbismuth.runtime

make -f "Makefile.library.mak" %ACTION% TARGET=%TARGET% ASSEMBLY=testbed.blib DO_VERSION=%DO_VERSION% ADDL_INC_FLAGS="%INC_CORE_RT% -Ibismuth.plugin.ui.standard\src -Ibismuth.plugin.audio.openal\src" ADDL_LINK_FLAGS="%LNK_CORE_RT% -lbismuth.plugin.ui.standard -lbismuth.plugin.audio.openal"
IF %ERRORLEVEL% NEQ 0 (echo Error:%ERRORLEVEL% && exit)
