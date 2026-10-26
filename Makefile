CC ?= cc
AR ?= ar
GLSLC ?= glslc
BUILD ?= debug
CPPFLAGS += -Iinclude -DGLFW_INCLUDE_NONE
CFLAGS += -std=c11 -Wall -Wextra -Wno-unused-parameter -MMD -MP
ifeq ($(BUILD),release)
CFLAGS += -O2 -DNDEBUG
else
CFLAGS += -g -O0 -DDEBUG
endif
OBJ_DIR := build/obj/$(BUILD)
SRCS := $(shell find src -name '*.c' | sort)
OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(SRCS))
EXAMPLES := examples/main.c examples/starter.c
EXAMPLE_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(EXAMPLES))
LIB := build/libgraphics_app_$(BUILD).a
.DEFAULT_GOAL := all
OUTPUT := build/app-$(BUILD).out
.PHONY: all run clean shader check test
all: $(OUTPUT)
	ln -sfn app-$(BUILD).out build/app.out
shader: shaders/vert.spv shaders/frag.spv
shaders/vert.spv: shaders/tri.vert
	$(GLSLC) $< -o $@
shaders/frag.spv: shaders/tri.frag
	$(GLSLC) $< -o $@
$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
$(LIB): $(OBJS)
	@mkdir -p build
	$(AR) rcs $@ $^
$(OUTPUT): $(EXAMPLE_OBJS) $(LIB) shaders/vert.spv shaders/frag.spv
	$(CC) $(EXAMPLE_OBJS) $(LIB) -o $@ -lglfw -lvulkan -lm
run: all
	./$(OUTPUT)
clean:
	rm -rf build shaders/vert.spv shaders/frag.spv
check:
	python3 tools/check_boundaries.py
-include $(OBJS:.o=.d) $(EXAMPLE_OBJS:.o=.d)

build/ui_input_state_test: tests/ui_input_state_test.c src/ui/ui.c include/ui/ui.h
	@mkdir -p build
	$(CC) $(CPPFLAGS) -std=c11 -Wall -Wextra -Wno-unused-parameter -ffunction-sections -fdata-sections $< -Wl,--gc-sections -lm -o $@
test: check build/ui_input_state_test
	./build/ui_input_state_test
