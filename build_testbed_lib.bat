
make -f "Makefile.library.mak" %ACTION% TARGET=%TARGET% ASSEMBLY=testbed.lib DO_VERSION=%DO_VERSION% ADDL_INC_FLAGS="-Ibismuth.engine\src -Ibismuth.plugin.ui.standard\src -Ibismuth.plugin.audio.openal\src" ADDL_LINK_FLAGS="-lbismuth.engine -lbismuth.plugin.ui.standard -lbismuth.plugin.audio.openal"
IF %ERRORLEVEL% NEQ 0 (echo Error:%ERRORLEVEL% && exit)
