%:: %,v
%:: RCS/%,v
%:: RCS/%
%:: s.%
%:: SCCS/s.%

BUILD_DIR := bin
OBJ_DIR := obj

# NOTE: ASSEMBLY must be set on calling this makefile

DEFINES := -DBIMPORT

# Detect OS and architecture
ifeq ($(OS),Windows_NT)
    # WIN32
	BUILD_PLATFORM := windows
	EXTENSION := .exe
	COMPILER_FLAGS := -Wall -Werror -Wno-error=deprecated-declarations -Wno-error=unused-function -Wvla -Werror=vla -Wgnu-folding-constant -Wno-missing-braces -fdeclspec -Wstrict-prototypes
	INCLUDE_FLAGS := -I$(ASSEMBLY)\src $(ADDL_INC_FLAGS)
	LINKER_FLAGS := -L$(BUILD_DIR) $(ADDL_LINK_FLAGS)
	DEFINES += -D_CRT_SECURE_NO_WARNINGS

# Make does not offer a recursive wildcard function, and Windows needs one, so here it is:
	rwildcard=$(wildcard $1$2) $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2))
	DIR := $(subst /,\,${CURDIR})

	# .c files
	SRC_FILES := $(call rwildcard,$(ASSEMBLY)/,*.c)
	# directories with .h files
	DIRECTORIES := \$(ASSEMBLY)\src $(subst $(DIR),,$(shell dir $(ASSEMBLY)\src /S /AD /B | findstr /i src)) 
	OBJ_FILES := $(SRC_FILES:%=$(OBJ_DIR)/%.o)
    ifeq ($(PROCESSOR_ARCHITEW6432),AMD64)
        # AMD64
    else
        ifeq ($(PROCESSOR_ARCHITECTURE),AMD64)
            # AMD64
        endif
        ifeq ($(PROCESSOR_ARCHITECTURE),x86)
            # IA32
        endif
    endif
endif

# Defaults to debug unless release is specified
ifeq ($(TARGET),release)
# release
DEFINES += -DBRELEASE
COMPILER_FLAGS += -MD -O2
else
# debug
DEFINES += -D_DEBUG
COMPILER_FLAGS += -g -MD -O0
LINKER_FLAGS += -g
endif

all: scaffold compile link

.NOTPARALLEL: scaffold

.PHONY: scaffold
scaffold: # create build directory
ifeq ($(BUILD_PLATFORM),windows)
	-@setlocal enableextensions enabledelayedexpansion && mkdir $(addprefix $(OBJ_DIR), $(DIRECTORIES)) 2>NUL || cd .
	-@setlocal enableextensions enabledelayedexpansion && mkdir $(BUILD_DIR) 2>NUL || cd .
else
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(addprefix $(OBJ_DIR)/,$(DIRECTORIES))
endif

.PHONY: link
link: scaffold $(OBJ_FILES) # link
	@echo Linking "$(ASSEMBLY)"...
ifeq ($(BUILD_PLATFORM),windows)
	@clang $(OBJ_FILES) -o $(BUILD_DIR)\$(ASSEMBLY)$(EXTENSION) $(LINKER_FLAGS)
else
	@clang $(OBJ_FILES) -o $(BUILD_DIR)/$(ASSEMBLY)$(EXTENSION) $(LINKER_FLAGS)
endif

.PHONY: compile
compile:
	@echo --- Performing "$(ASSEMBLY)" $(TARGET) build ---
-include $(OBJ_FILES:.o=.d)

.PHONY: clean
clean: # clean build directory
	@echo --- Cleaning "$(ASSEMBLY)" ---
ifeq ($(BUILD_PLATFORM),windows)
	@if exist $(BUILD_DIR)\$(ASSEMBLY)$(EXTENSION) del $(BUILD_DIR)\$(ASSEMBLY)$(EXTENSION)
# Windows builds include a lot of files... get them all
	@if exist $(BUILD_DIR)\$(ASSEMBLY).* del $(BUILD_DIR)\$(ASSEMBLY).*
	@if exist $(OBJ_DIR)\$(ASSEMBLY) rmdir /s /q $(OBJ_DIR)\$(ASSEMBLY)
else
	@rm -rf $(BUILD_DIR)/$(ASSEMBLY)$(EXTENSION)
	@rm -rf $(OBJ_DIR)/$(ASSEMBLY)
endif

# compile .c to .o object
$(OBJ_DIR)/%.c.o: %.c 
	@echo   $<...
	@clang $< $(COMPILER_FLAGS) -c -o $@ $(DEFINES) $(INCLUDE_FLAGS)

-include $(OBJ_FILES:.o=.d)
