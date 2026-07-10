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

IWYU := include-what-you-use
IWYU_FLAGS := $(INC_FLAGS) -std=c++23

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


.PHONY: iwyu
iwyu:
	@for src in $(SRCS); do\
		echo "== $$src =="; \
		$(IWYU) $(IWYU_FLAGS) -Xiwyu --error_always $$src 2>&1 | grep -v "^$$"; \
	done

.PHONY: iwyu-fix
iwyu-fix:
	@$(IWYU) $(IWYU_FLAGS) $(SRCS) 2>&1 | fix_includes.py


clean:
	@$(MAKE) deldir -i -s
	@$(MAKE) rmExecs -i -s
	@$(MAKE) rmFiles -i -s
	clear


deldir:
	@rm -r $(BUILD_DIR)

rmExecs:
	@rm ./out
	@find . -maxdepth 1 -type f -executable -exec rm -i {} +

rmFiles:
	@rm *.o *.asm original.txt

run:	
	@valgrind --leak-check=full $(BUILD_DIR)/$(TARGET_EXEC)

test:
	@./$(TARGET_EXEC) -ast ./tests/test.z
	@./out

-include $(DEPS)
