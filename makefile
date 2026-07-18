ifeq ($(MAKECMDGOALS),)
	BUILD_MODE := live
	TARGET_EXEC := zcp
else ifneq (,$(filter live all,$(MAKECMDGOALS)))
	BUILD_MODE := live
	TARGET_EXEC := zcp
else
	BUILD_MODE := indev
	TARGET_EXEC := zcp-dev
endif

BUILD_DIR := ./build
SRC_DIRS := ./src

SRCS := $(shell find $(SRC_DIRS) -name '*.cpp')
OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

INC_DIRS := $(shell find $(SRC_DIRS) -type d)
INC_FLAGS := $(addprefix -I,$(INC_DIRS))

COMMON_FLAGS := $(INC_FLAGS) -MMD -MP -O2 -std=c++23

ifeq ($(BUILD_MODE),live)
	CPPFLAGS := $(COMMON_FLAGS)
else
	CPPFLAGS := $(COMMON_FLAGS) -Wextra -Wall -g
endif


.PHONY: all live clean deldir rmExecs rmFiles run test

all: live

indev: $(TARGET_EXEC)

live: $(TARGET_EXEC)
	@$(MAKE) deldir -i -s
	@echo "Usage like: ./zcp <flags> <file.z>"
	@echo "Help flag (currently needs some file): ./zcp -h <file.z>"

$(TARGET_EXEC): $(OBJS)
	@$(CXX) $(OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.cpp.o: %.cpp
	@mkdir -p $(dir $@)
	@$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@


IWYU := include-what-you-use
IWYU_TOOL := iwyu_tool.py
FIX_INCLUDES := fix_includes.py
NPROC := $(shell nproc)

CHANGED_SRCS := $(shell git diff --name-only --diff-filter=ACMR main -- '*.cpp' '*.h' 2>/dev/null)
CHANGED_HEADERS := $(filter %.h,$(CHANGED_SRCS))
CHANGED_CPPS := $(filter %.cpp,$(CHANGED_SRCS))
HAS_DEPFILES := $(shell find $(BUILD_DIR) -name '*.cpp.o.d' 2>/dev/null | head -1)

# Only attempt affected-header lookup if dep files actually exist
AFFECTED_CPPS := $(if $(and $(CHANGED_HEADERS),$(HAS_DEPFILES)),$(shell ./scripts/iwyu-affected-cpps.sh $(CHANGED_HEADERS)))
ALL_TARGET_CPPS := $(sort $(CHANGED_CPPS) $(AFFECTED_CPPS))

# True if headers changed but we have no dep files to trace their impact —
# incremental scan would be unsafe/incomplete, so we must fall back.
NEEDS_FULL_FALLBACK := $(and $(CHANGED_HEADERS),$(if $(HAS_DEPFILES),,1))

# Full sweep: clean rebuild + full-tree IWYU pass. Use periodically/pre-release.
.PHONY: iwyu-full
iwyu-full:
	@$(MAKE) clean -i -s
	@bear -- $(MAKE) indev -j$(NPROC)
	@$(IWYU_TOOL) -p . -j $(NPROC)

.PHONY: iwyu-full-fix
iwyu-full-fix:
	@$(MAKE) clean -i -s
	@bear -- $(MAKE) indev -j$(NPROC)
	-@$(IWYU_TOOL) -p . -j $(NPROC) | $(FIX_INCLUDES)

.PHONY: iwyu-changed
iwyu-changed:
ifneq ($(NEEDS_FULL_FALLBACK),)
	@echo "No .d files found but headers changed — falling back to full scan."
	@$(MAKE) iwyu-full
else
ifeq ($(ALL_TARGET_CPPS),)
	@echo "No changed or affected .cpp files vs main."
else
	@bear -- $(MAKE) indev -j$(NPROC)
	@$(IWYU_TOOL) -p . -j $(NPROC) $(ALL_TARGET_CPPS)
endif
endif

.PHONY: iwyu-changed-fix
iwyu-changed-fix:
ifneq ($(NEEDS_FULL_FALLBACK),)
	@echo "No .d files found but headers changed — falling back to full scan+fix."
	@$(MAKE) iwyu-full-fix
else
ifeq ($(ALL_TARGET_CPPS),)
	@echo "No changed or affected .cpp files vs main."
else
	@bear -- $(MAKE) indev -j$(NPROC)
	-@$(IWYU_TOOL) -p . -j $(NPROC) $(ALL_TARGET_CPPS) | $(FIX_INCLUDES)
endif
endif

clean:
	@$(MAKE) deldir -i -s
	@$(MAKE) rmExecs -i -s
	@$(MAKE) rmFiles -i -s
	@rm compile_commands.json
	@rm compile_log.txt
	clear


deldir:
	@rm -r $(BUILD_DIR)

rmExecs:
	@rm ./out
	@find . -maxdepth 1 -type f -executable -exec rm -i {} +

rmFiles:
	@rm *.o *.asm original.txt compilation_log.txt

val:	
	@valgrind --leak-check=full ./$(TARGET_EXEC) -d tests/test.z

test:
	@./$(TARGET_EXEC) -d ./tests/test.z
	@./out

-include $(DEPS)
