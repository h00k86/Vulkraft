CC ?= cc
AR ?= ar
GLSLC ?= glslc
BUILD ?= debug
PLATFORM ?= glfw
ifeq ($(filter $(PLATFORM),glfw sdl2 headless),)
$(error PLATFORM must be glfw, sdl2 or headless)
endif
PLATFORM_SUFFIX := $(if $(filter-out glfw,$(PLATFORM)),-$(PLATFORM),)
ifeq ($(PLATFORM),sdl2)
PLATFORM_LIBS := $(shell pkg-config --libs 'sdl2 >= 2.26.0')
CPPFLAGS += $(shell pkg-config --cflags 'sdl2 >= 2.26.0')
else
PLATFORM_LIBS := $(if $(filter glfw,$(PLATFORM)),-lglfw,)
endif
CPPFLAGS += -Iinclude
# Independently compiled UI modules, also linked by white-box core tests.
LINKED_UI_TESTS := ui_navigation_test ui_interaction_test ui_tabs_splitter_test ui_basic_widgets_test ui_text_edit_test ui_text_edit_app_test ui_renderer_test ui_frame_input_test ui_draw_list_test config_editor_ui_test ui_color_picker_test
UI_STANDALONE_LIB := build/libui_$(BUILD).a
UI_CORE_MODULE_SRCS := src/ui/ui_buttons.c src/ui/ui_numeric_controls.c src/ui/ui_text_field.c src/ui/ui_capture.c src/ui/ui_menu.c src/ui/ui_dropdown.c src/ui/ui_modals.c src/ui/ui_layout.c src/ui/ui_properties.c src/ui/ui_panels.c src/ui/ui_text_edit.c src/ui/ui_navigation.c src/ui/ui_theme.c src/ui/ui_color_picker.c src/ui/ui_progress.c
CFLAGS += -std=c11 -Wall -Wextra -Wno-unused-parameter -MMD -MP
ifeq ($(BUILD),release)
CFLAGS += -O2 -DNDEBUG
else
CFLAGS += -g -O0 -DDEBUG
endif
OBJ_DIR := build/obj/$(BUILD)/$(PLATFORM)
ALL_SRCS := $(shell find src -name '*.c' | sort)
SRCS := $(filter-out src/platform/backends/% src/render/backends/%,$(ALL_SRCS)) src/platform/backends/$(PLATFORM).c src/render/backends/$(PLATFORM)_surface.c
OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(SRCS))
EXAMPLES := examples/main.c examples/starter.c
EXAMPLE_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(EXAMPLES))
LIB := build/libgraphics_app_$(BUILD)$(if $(filter-out glfw,$(PLATFORM)),_$(PLATFORM),).a
.DEFAULT_GOAL := all
OUTPUT := build/app-$(BUILD)$(if $(filter-out glfw,$(PLATFORM)),-$(PLATFORM),).out

# Benchmarks use the release core, independently of the selected app build.
BENCH_ALLOC_FLAGS := -fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free -Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=free
.PHONY: benchmark-ui validate validate-native
benchmark-ui:
	$(MAKE) BUILD=release build/ui-benchmark.out build/ui-editor-benchmark.out
	./build/ui-benchmark.out
	./build/ui-editor-benchmark.out
build/ui-editor-benchmark.out: tools/ui_editor_benchmark.c tools/ui_benchmark_metrics.h tools/ui_benchmark_domains.h examples/config_editor/document.c examples/config_editor/editor.c examples/config_editor/view.c $(wildcard examples/config_editor/*.h src/ui/ui_state_*_internal.h) build/libui_picker_release.a build/libui_release.a
	$(CC) $(CPPFLAGS) -std=c11 -Wall -Wextra -Wno-unused-parameter -O2 $(BENCH_ALLOC_FLAGS) tools/ui_editor_benchmark.c examples/config_editor/document.c examples/config_editor/editor.c examples/config_editor/view.c build/libui_picker_release.a build/libui_release.a -lm -o $@
build/ui-benchmark.out: tools/ui_benchmark.c build/libui_release.a
	$(CC) $(CPPFLAGS) -std=c11 -Wall -Wextra -Wno-unused-parameter -O2 $< build/libui_release.a -Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=free -lm -o $@
validate:
	python3 tools/validate_project.py
validate-native:
	python3 tools/validate_project.py --native
.PHONY: all run clean shader check test
all: $(OUTPUT)
	ln -sfn $(notdir $(OUTPUT)) build/app.out
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
	$(CC) $(EXAMPLE_OBJS) $(LIB) -o $@ $(PLATFORM_LIBS) -lvulkan -lm
run: all
	./$(OUTPUT)
clean:
	rm -rf build shaders/vert.spv shaders/frag.spv
check:
	python3 tools/check_boundaries.py
-include $(OBJS:.o=.d) $(EXAMPLE_OBJS:.o=.d)

build/ui_input_state_test: tests/ui_input_state_test.c src/ui/ui.c $(wildcard src/ui/*.h) src/ui/ui_app_adapter.c tests/ui_app_test_adapter.h tests/ui_test_renderer.h $(wildcard include/*/*.h) $(UI_CORE_MODULE_SRCS)
	@mkdir -p build
	$(CC) $(CPPFLAGS) -std=c11 -Wall -Wextra -Wno-unused-parameter -ffunction-sections -fdata-sections $< $(UI_CORE_MODULE_SRCS) -Wl,--gc-sections -lm -o $@
TEXT_EDIT_FAULT_FLAGS := -DUI_TEXT_TEST_FAULT_ALLOC -Wl,--wrap=malloc
TEST_FLAGS := -std=c11 -Wall -Wextra -Wno-unused-parameter -g -ffunction-sections -fdata-sections
build/ui_memory_safety_test: tests/ui_memory_safety_test.c src/input/keyboard.c src/ui/ui.c $(wildcard src/ui/*.h) src/ui/ui_app_adapter.c tests/ui_app_test_adapter.h tests/ui_test_renderer.h $(wildcard include/*/*.h) $(UI_CORE_MODULE_SRCS)
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(TEST_FLAGS) $< $(UI_CORE_MODULE_SRCS) -Wl,--gc-sections -lm -o $@
build/texture_reload_test: tests/texture_reload_test.c src/render/texture_manager.c $(wildcard include/*/*.h)
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(TEST_FLAGS) $< -Wl,--gc-sections -lm -o $@
build/texture_reload_failure_test: tests/texture_reload_failure_test.c src/render/texture_manager.c $(LIB) $(wildcard include/*/*.h)
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(TEST_FLAGS) $< $(LIB) -lvulkan $(PLATFORM_LIBS) -lm -o $@
test: check build/ui_input_state_test build/ui_memory_safety_test build/texture_reload_test build/texture_reload_failure_test
	./build/ui_input_state_test
	./build/ui_memory_safety_test
	./build/texture_reload_test
	./build/texture_reload_failure_test

.PHONY: test-sanitize
build/ui_memory_safety_test_sanitize: tests/ui_memory_safety_test.c src/input/keyboard.c src/ui/ui.c $(wildcard src/ui/*.h) src/ui/ui_app_adapter.c tests/ui_app_test_adapter.h tests/ui_test_renderer.h $(wildcard include/*/*.h) $(UI_CORE_MODULE_SRCS)
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(TEST_FLAGS) -O1 -fsanitize=address,undefined -fno-omit-frame-pointer $< $(UI_CORE_MODULE_SRCS) -Wl,--gc-sections -lm -o $@
build/texture_reload_test_sanitize: tests/texture_reload_test.c src/render/texture_manager.c $(wildcard include/*/*.h)
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(TEST_FLAGS) -O1 -fsanitize=address,undefined -fno-omit-frame-pointer $< -Wl,--gc-sections -lm -o $@
test-sanitize: build/ui_memory_safety_test_sanitize build/texture_reload_test_sanitize
	ASAN_OPTIONS=detect_leaks=0 ./build/ui_memory_safety_test_sanitize
	ASAN_OPTIONS=detect_leaks=0 ./build/texture_reload_test_sanitize

DROPDOWN_TEST_ALLOC_FLAGS := -Wl,--wrap=realloc -Wl,--wrap=free
UI_STORAGE_TEST_ALLOC_FLAGS := -DUI_STORAGE_TEST_ALLOC -Wl,--wrap=realloc -Wl,--wrap=free
MEDIUM_TESTS := ui_error_test ui_session_test ui_navigation_batch_test ui_menu_storage_test ui_dropdown_storage_test ui_state_contract_test ui_overlay_modules_test ui_layout_modules_test ui_text_edit_module_test ui_navigation_module_test ui_interaction_test ui_basic_widgets_test ui_tabs_splitter_test ui_text_edit_test ui_text_edit_app_test ui_navigation_test ui_draw_list_test ui_renderer_test ui_frame_input_test ui_context_test utf8_editor_test utf8_render_test app_loop_failure_test render_failure_test keyboard_event_test ui_medium_test font_parser_test path_containment_test swapchain_policy_test init_failure_test buffer_failure_test texture_upload_failure_test
MEDIUM_BINS := $(addprefix build/,$(MEDIUM_TESTS))
build/ui_medium_test: tests/ui_medium_test.c tests/ui_memory_safety_test.c src/input/keyboard.c src/ui/ui.c $(wildcard src/ui/*.h) src/ui/ui_app_adapter.c tests/ui_app_test_adapter.h tests/ui_test_renderer.h $(wildcard include/*/*.h) $(UI_CORE_MODULE_SRCS)
	$(CC) $(CPPFLAGS) $(TEST_FLAGS) $< $(UI_CORE_MODULE_SRCS) -Wl,--gc-sections -lm -o $@
build/font_parser_test: tests/font_parser_test.c src/render/font.c include/render/font.h
	$(CC) $(CPPFLAGS) $(TEST_FLAGS) tests/font_parser_test.c src/render/font.c -lm -o $@
build/path_containment_test: tests/path_containment_test.c src/platform/platform_fs.c include/platform/platform_fs.h
	$(CC) $(CPPFLAGS) $(TEST_FLAGS) tests/path_containment_test.c src/platform/platform_fs.c -o $@
build/swapchain_policy_test: tests/swapchain_policy_test.c src/render/vulkan_swapchain.c $(wildcard include/*/*.h)
	$(CC) $(CPPFLAGS) $(TEST_FLAGS) $< -Wl,--gc-sections -lm -o $@
.PHONY: test-medium test-app-resources test-app-resources-sanitize
build/app_resources_test: tests/app_resources_test.c src/app/resources.c src/platform/paths.c src/platform/platform_fs.c $(wildcard include/*/*.h)
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(TEST_FLAGS) $< src/app/resources.c src/platform/paths.c src/platform/platform_fs.c -o $@
build/app_resources_test_sanitize: tests/app_resources_test.c src/app/resources.c src/platform/paths.c src/platform/platform_fs.c $(wildcard include/*/*.h)
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(TEST_FLAGS) -O1 -fsanitize=address,undefined -fno-omit-frame-pointer $< src/app/resources.c src/platform/paths.c src/platform/platform_fs.c -o $@
test-app-resources: build/app_resources_test
	./build/app_resources_test
test: test-app-resources
test-app-resources-sanitize: build/app_resources_test_sanitize
	ASAN_OPTIONS=detect_leaks=0 ./build/app_resources_test_sanitize
test-sanitize: test-app-resources-sanitize
test: test-medium
test-medium: $(MEDIUM_BINS)
	@set -e; for t in $(MEDIUM_BINS); do ./$$t; done

build/init_failure_test: tests/init_failure_test.c src/render/init_vulkan.c src/render/gfx.c $(wildcard include/*/*.h)
	$(CC) $(CPPFLAGS) $(TEST_FLAGS) $< -Wl,--gc-sections -lm -o $@

build/buffer_failure_test: tests/buffer_failure_test.c src/render/vulkan_buffer.c src/render/vulkan_uniform_buffer.c $(wildcard include/*/*.h)
	$(CC) $(CPPFLAGS) $(TEST_FLAGS) $< -Wl,--gc-sections -lm -o $@

MEDIUM_SANITIZERS := $(addsuffix _sanitize,$(MEDIUM_BINS))
$(MEDIUM_SANITIZERS): build/%_sanitize: tests/%.c $(wildcard src/*/*.c src/ui/*.h include/*/*.h) tests/ui_app_test_adapter.h tests/ui_test_renderer.h tests/ui_memory_safety_test.c tests/ui_renderer_test.c
	$(CC) $(CPPFLAGS) $(TEST_FLAGS) -O1 -fsanitize=address,undefined -fno-omit-frame-pointer $< $(if $(filter $(LINKED_UI_TESTS),$*),src/ui/ui.c $(UI_CORE_MODULE_SRCS) $(if $(filter-out ui_draw_list_test,$*),src/ui/ui_draw_list.c)) $(if $(filter ui_text_edit_module_test,$*),src/ui/ui_text_edit.c $(TEXT_EDIT_FAULT_FLAGS)) $(if $(filter ui_navigation_module_test ui_layout_modules_test ui_overlay_modules_test ui_state_contract_test ui_dropdown_storage_test ui_navigation_batch_test ui_menu_storage_test ui_error_test ui_session_test,$*),$(UI_STANDALONE_SRCS)) $(if $(filter ui_context_test utf8_editor_test keyboard_event_test ui_medium_test,$*),$(UI_CORE_MODULE_SRCS)) $(if $(filter ui_text_edit_app_test,$*),src/input/keyboard.c src/input/scroll.c src/platform/backends/headless.c) $(if $(filter font_parser_test utf8_render_test,$*),src/render/font.c) $(if $(filter path_containment_test,$*),src/platform/platform_fs.c) $(if $(filter render_failure_test,$*),src/math/matrix.c) $(if $(filter texture_upload_failure_test,$*),-lvulkan) $(if $(filter ui_dropdown_storage_test,$*),$(DROPDOWN_TEST_ALLOC_FLAGS)) $(if $(filter ui_navigation_batch_test ui_menu_storage_test ui_error_test,$*),$(UI_STORAGE_TEST_ALLOC_FLAGS)) $(if $(filter ui_session_test,$*),$(BENCH_ALLOC_FLAGS)) -Wl,--gc-sections -lm -o $@
test-sanitize: test-medium-sanitize
.PHONY: test-medium-sanitize
test-medium-sanitize: $(MEDIUM_SANITIZERS)
	@set -e; for t in $(MEDIUM_SANITIZERS); do ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 ./$$t; done

build/texture_upload_failure_test: tests/texture_upload_failure_test.c src/render/vulkan_texture.c $(wildcard include/*/*.h)
	$(CC) $(CPPFLAGS) $(TEST_FLAGS) $< -Wl,--gc-sections -lvulkan -lm -o $@

# Opt-in GUI scene; never launched by the headless test target.
build/runtime-validation.out: tests/runtime_scene.c examples/main.c $(LIB) shaders/vert.spv shaders/frag.spv
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/runtime_scene.c examples/main.c $(LIB) $(PLATFORM_LIBS) -lvulkan -lm -o $@

# Real GLFW/X11 event queue, hidden window; no desktop input automation.
build/glfw-key-event-probe.out: tests/glfw_key_event_probe.c src/input/keyboard.c src/ui/ui.c $(wildcard src/ui/*.h) src/ui/ui_app_adapter.c tests/ui_app_test_adapter.h tests/ui_test_renderer.h src/platform/backends/glfw.c src/platform/backends/glfw_private.h $(wildcard include/*/*.h) $(UI_CORE_MODULE_SRCS)
	$(CC) $(CPPFLAGS) $(TEST_FLAGS) $< $(UI_CORE_MODULE_SRCS) src/input/keyboard.c src/platform/backends/glfw.c -Wl,--gc-sections -lglfw -lX11 -lm -o $@

build/keyboard_event_test: tests/keyboard_event_test.c tests/ui_memory_safety_test.c src/ui/ui.c $(wildcard src/ui/*.h) src/ui/ui_app_adapter.c tests/ui_app_test_adapter.h tests/ui_test_renderer.h src/input/keyboard.c $(wildcard include/*/*.h) $(UI_CORE_MODULE_SRCS)
	$(CC) $(CPPFLAGS) $(TEST_FLAGS) $< $(UI_CORE_MODULE_SRCS) -Wl,--gc-sections -lm -o $@

build/render_failure_test: tests/render_failure_test.c src/render/vulkan_draw.c src/render/vulkan_command.c src/math/matrix.c $(wildcard include/*/*.h)
	$(CC) $(CPPFLAGS) $(TEST_FLAGS) $< src/math/matrix.c -Wl,--gc-sections -lm -o $@

build/app_loop_failure_test: tests/app_loop_failure_test.c src/app/application.c src/app/application_legacy.c $(wildcard include/*/*.h)
	$(CC) $(CPPFLAGS) $(TEST_FLAGS) $< -Wl,--gc-sections -lm -o $@

build/utf8_editor_test: tests/utf8_editor_test.c tests/ui_memory_safety_test.c src/ui/ui.c $(wildcard src/ui/*.h) src/ui/ui_app_adapter.c tests/ui_app_test_adapter.h tests/ui_test_renderer.h src/input/keyboard.c $(wildcard include/*/*.h) $(UI_CORE_MODULE_SRCS)
	$(CC) $(CPPFLAGS) $(TEST_FLAGS) $< $(UI_CORE_MODULE_SRCS) -Wl,--gc-sections -lm -o $@

build/utf8_render_test: tests/utf8_render_test.c src/render/text_renderer.c src/render/font.c $(wildcard include/*/*.h)
	$(CC) $(CPPFLAGS) $(TEST_FLAGS) $< src/render/font.c -Wl,--gc-sections -lm -o $@

PLATFORM_TEST_SOURCES := src/platform/window.c src/platform/backends/headless.c src/input/keyboard.c src/input/mouse.c src/input/scroll.c
build/platform_headless_test: tests/platform_headless_test.c src/ui/ui.c $(wildcard src/ui/*.h) src/ui/ui_app_adapter.c tests/ui_app_test_adapter.h tests/ui_test_renderer.h $(PLATFORM_TEST_SOURCES) $(wildcard include/*/*.h) $(UI_CORE_MODULE_SRCS)
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(TEST_FLAGS) $< $(UI_CORE_MODULE_SRCS) $(PLATFORM_TEST_SOURCES) -Wl,--gc-sections -lm -o $@
build/platform_headless_test_sanitize: tests/platform_headless_test.c src/ui/ui.c $(wildcard src/ui/*.h) src/ui/ui_app_adapter.c tests/ui_app_test_adapter.h tests/ui_test_renderer.h $(PLATFORM_TEST_SOURCES) $(wildcard include/*/*.h) $(UI_CORE_MODULE_SRCS)
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(TEST_FLAGS) -O1 -fsanitize=address,undefined -fno-omit-frame-pointer $< $(UI_CORE_MODULE_SRCS) $(PLATFORM_TEST_SOURCES) -Wl,--gc-sections -lm -o $@
.PHONY: test-platform test-platform-sanitize
test-platform: build/platform_headless_test
	./build/platform_headless_test
test-platform-sanitize: build/platform_headless_test_sanitize
	ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 ./build/platform_headless_test_sanitize
test: test-platform
test-sanitize: test-platform-sanitize

# Adapter-only check requires GLFW headers, but does not open a window.
build/platform_glfw_mapping_test: tests/platform_glfw_mapping_test.c src/platform/backends/glfw.c src/platform/backends/glfw_private.h $(wildcard include/platform/*.h)
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(TEST_FLAGS) $< -Wl,--gc-sections -o $@
.PHONY: test-platform-glfw
test-platform-glfw: build/platform_glfw_mapping_test
	./build/platform_glfw_mapping_test
ifeq ($(PLATFORM),glfw)
test: test-platform-glfw
endif

build/ui_context_test: tests/ui_context_test.c tests/ui_memory_safety_test.c src/ui/ui.c $(wildcard src/ui/*.h) src/ui/ui_app_adapter.c tests/ui_app_test_adapter.h tests/ui_test_renderer.h src/input/keyboard.c $(wildcard include/*/*.h) $(UI_CORE_MODULE_SRCS)
	$(CC) $(CPPFLAGS) $(TEST_FLAGS) $< $(UI_CORE_MODULE_SRCS) -Wl,--gc-sections -lm -o $@

build/ui_frame_input_test: tests/ui_frame_input_test.c tests/ui_test_renderer.h src/ui/ui.c $(wildcard src/ui/*.h include/*/*.h) $(UI_CORE_MODULE_SRCS) $(UI_STANDALONE_LIB)
	$(CC) $(CPPFLAGS) $(TEST_FLAGS) $< $(UI_STANDALONE_LIB) -Wl,--gc-sections -lm -o $@

build/ui_renderer_test: tests/ui_renderer_test.c src/ui/ui.c $(wildcard src/ui/*.h include/*/*.h) $(UI_CORE_MODULE_SRCS) $(UI_STANDALONE_LIB)
	$(CC) $(CPPFLAGS) $(TEST_FLAGS) $< $(UI_STANDALONE_LIB) -Wl,--gc-sections -lm -o $@

build/ui_draw_list_test: tests/ui_draw_list_test.c tests/ui_renderer_test.c src/ui/ui_draw_list.c src/ui/ui.c $(wildcard src/ui/*.h include/*/*.h) $(UI_CORE_MODULE_SRCS) $(UI_STANDALONE_LIB)
	$(CC) $(CPPFLAGS) $(TEST_FLAGS) $< $(UI_STANDALONE_LIB) -Wl,--gc-sections -lm -o $@

build/draw-list-runtime.out: tests/ui_draw_list_runtime.c tests/runtime_scene.c examples/main.c $(LIB) shaders/vert.spv shaders/frag.spv
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/ui_draw_list_runtime.c $(LIB) $(PLATFORM_LIBS) -lvulkan -lm -o $@

# Public API consumer, linked normally: no section garbage collection to hide dependencies.
UI_STANDALONE_SRCS := src/ui/ui.c src/ui/ui_draw_list.c $(UI_CORE_MODULE_SRCS)
UI_STANDALONE_OBJ_DIR := build/obj/ui-standalone/$(BUILD)
UI_STANDALONE_OBJS := $(patsubst %.c,$(UI_STANDALONE_OBJ_DIR)/%.o,$(UI_STANDALONE_SRCS))
UI_STANDALONE_BIN := build/ui-standalone-$(BUILD).out
$(UI_STANDALONE_OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
$(UI_STANDALONE_LIB): $(UI_STANDALONE_OBJS)
	$(RM) $@.tmp
	$(AR) rcs $@.tmp $^
	mv $@.tmp $@
$(UI_STANDALONE_BIN): $(UI_STANDALONE_OBJ_DIR)/examples/ui_standalone.o $(UI_STANDALONE_LIB)
	$(CC) $^ -lm -o $@
-include $(UI_STANDALONE_OBJS:.o=.d) $(UI_STANDALONE_OBJ_DIR)/examples/ui_standalone.d
.PHONY: ui-standalone test-ui-standalone
ui-standalone: $(UI_STANDALONE_BIN)
test-ui-standalone: $(UI_STANDALONE_BIN)
	./$(UI_STANDALONE_BIN)
test: test-ui-standalone
build/ui-standalone-sanitize.out: examples/ui_standalone.c $(UI_STANDALONE_SRCS) $(wildcard src/ui/*.h include/*/*.h)
	@mkdir -p build
	$(CC) $(CPPFLAGS) -std=c11 -Wall -Wextra -Wno-unused-parameter -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer examples/ui_standalone.c $(UI_STANDALONE_SRCS) -lm -o $@
.PHONY: test-ui-standalone-sanitize
test-ui-standalone-sanitize: build/ui-standalone-sanitize.out
	ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 ./build/ui-standalone-sanitize.out
test-sanitize: test-ui-standalone-sanitize

# Optional filesystem picker: link before libui; core never refers back to it.
UI_PICKER_MODULE_SRCS := src/ui/ui_file_picker.c src/platform/paths.c src/platform/platform_fs.c
UI_PICKER_MODULE_OBJS := $(patsubst %.c,$(UI_STANDALONE_OBJ_DIR)/%.o,$(UI_PICKER_MODULE_SRCS))
UI_PICKER_MODULE_LIB := build/libui_picker_$(BUILD).a
$(UI_PICKER_MODULE_LIB): $(UI_PICKER_MODULE_OBJS)
	$(RM) $@.tmp
	$(AR) rcs $@.tmp $^
	mv $@.tmp $@
-include $(UI_PICKER_MODULE_OBJS:.o=.d)
.PHONY: ui-picker
ui-picker: $(UI_PICKER_MODULE_LIB) $(UI_STANDALONE_LIB)

UI_PICKER_TEST_SRCS := src/ui/ui.c $(UI_CORE_MODULE_SRCS) src/ui/ui_file_picker.c src/platform/paths.c src/platform/platform_fs.c
build/ui_file_picker_test: tests/ui_file_picker_test.c $(UI_PICKER_MODULE_LIB) $(UI_STANDALONE_LIB)
	$(CC) $(CPPFLAGS) $(TEST_FLAGS) $< $(UI_PICKER_MODULE_LIB) $(UI_STANDALONE_LIB) -Wl,--wrap=calloc -lm -o $@
build/ui_file_picker_test_sanitize: tests/ui_file_picker_test.c $(UI_PICKER_TEST_SRCS) $(wildcard src/ui/*.h include/*/*.h)
	$(CC) $(CPPFLAGS) $(TEST_FLAGS) -O1 -fsanitize=address,undefined -fno-omit-frame-pointer $< $(UI_PICKER_TEST_SRCS) -Wl,--wrap=calloc -lm -o $@
.PHONY: test-ui-picker test-ui-picker-sanitize
test-ui-picker: build/ui_file_picker_test
	./build/ui_file_picker_test
test-ui-picker-sanitize: build/ui_file_picker_test_sanitize
	ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 ./build/ui_file_picker_test_sanitize
test: test-ui-picker
test-sanitize: test-ui-picker-sanitize

# Optional SDL2 software UI; not part of the GLFW/Vulkan application archive.
SDL_UI_FLAGS = $(shell pkg-config --cflags sdl2 SDL2_image freetype2)
SDL_UI_LIBS = $(shell pkg-config --libs sdl2 SDL2_image freetype2)
SDL_UI_OBJDIR := build/obj/ui-sdl2/$(BUILD)
SDL_UI_OBJS := $(SDL_UI_OBJDIR)/ui_sdl2.o $(SDL_UI_OBJDIR)/ui_sdl2_input.o
SDL_UI_LIB := build/libui_sdl2_$(BUILD).a
$(SDL_UI_OBJDIR)/%.o: backends/sdl2/%.c include/backends/ui_sdl2.h $(wildcard include/ui/*.h include/text/*.h include/input/*.h include/platform/*.h)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SDL_UI_FLAGS) -c $< -o $@
$(SDL_UI_LIB): $(SDL_UI_OBJS)
	$(RM) $@.tmp
	$(AR) rcs $@.tmp $^
	mv $@.tmp $@
-include $(SDL_UI_OBJS:.o=.d)
.PHONY: ui-sdl2
ui-sdl2: $(SDL_UI_LIB) $(UI_STANDALONE_LIB)
build/ui-sdl2-test-$(BUILD).out: tests/ui_sdl2_test.c $(SDL_UI_LIB) $(UI_STANDALONE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SDL_UI_FLAGS) $^ $(SDL_UI_LIBS) -lm -o $@
.PHONY: test-ui-sdl2
test-ui-sdl2: build/ui-sdl2-test-$(BUILD).out
	SDL_VIDEODRIVER=dummy ./build/ui-sdl2-test-$(BUILD).out
build/ui-sdl2-sanitize.out: tests/ui_sdl2_test.c backends/sdl2/ui_sdl2.c backends/sdl2/ui_sdl2_input.c $(UI_STANDALONE_SRCS) $(wildcard include/backends/*.h include/ui/*.h src/ui/*.h)
	$(CC) $(CPPFLAGS) $(SDL_UI_FLAGS) -std=c11 -Wall -Wextra -Wno-unused-parameter -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer $(filter %.c,$^) $(SDL_UI_LIBS) -lm -o $@
.PHONY: test-ui-sdl2-sanitize
test-ui-sdl2-sanitize: build/ui-sdl2-sanitize.out
	SDL_VIDEODRIVER=dummy ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 ./build/ui-sdl2-sanitize.out
build/ui-reference-sdl2-$(BUILD).out: examples/ui_sdl2.c examples/ui_reference_scene.c examples/ui_reference_scene.h $(SDL_UI_LIB) $(UI_PICKER_MODULE_LIB) $(UI_STANDALONE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SDL_UI_FLAGS) $(filter %.c %.a,$^) $(SDL_UI_LIBS) -lm -o $@
build/ui-reference-vulkan-$(BUILD)$(PLATFORM_SUFFIX).out: examples/main.c examples/ui_reference_vulkan.c examples/ui_reference_scene.c examples/ui_reference_scene.h $(LIB) shaders/vert.spv shaders/frag.spv
	$(CC) $(CPPFLAGS) $(CFLAGS) $(filter %.c %.a,$^) $(PLATFORM_LIBS) -lvulkan -lm -o $@
.PHONY: ui-reference-sdl2 ui-reference-vulkan
ui-reference-sdl2: build/ui-reference-sdl2-$(BUILD).out
ui-reference-vulkan: build/ui-reference-vulkan-$(BUILD)$(PLATFORM_SUFFIX).out
.PHONY: ui-package
ui-package:
	python3 tools/package_ui.py

.PHONY: test-ui-package
test-ui-package: ui-package
	python3 tools/check_ui_package.py

# Keyboard navigation regression: core only, no native window or renderer.
build/ui_navigation_test: tests/ui_navigation_test.c src/ui/ui.c $(wildcard src/ui/*.h include/*/*.h) $(UI_CORE_MODULE_SRCS) $(UI_STANDALONE_LIB)
	$(CC) $(CPPFLAGS) $(TEST_FLAGS) $< $(UI_STANDALONE_LIB) -Wl,--gc-sections -lm -o $@

build/ui_text_edit_test: tests/ui_text_edit_test.c tests/ui_navigation_test.c src/ui/ui.c $(wildcard src/ui/*.h include/*/*.h) $(UI_CORE_MODULE_SRCS) $(UI_STANDALONE_LIB)
	$(CC) $(CPPFLAGS) $(TEST_FLAGS) $< $(UI_STANDALONE_LIB) -Wl,--gc-sections -lm -o $@
build/ui_text_edit_app_test: tests/ui_text_edit_app_test.c tests/ui_text_edit_test.c tests/ui_navigation_test.c src/ui/ui.c src/ui/ui_app_adapter.c src/input/keyboard.c src/input/scroll.c src/platform/backends/headless.c $(wildcard src/ui/*.h include/*/*.h) $(UI_CORE_MODULE_SRCS) $(UI_STANDALONE_LIB)
	$(CC) $(CPPFLAGS) $(TEST_FLAGS) $< src/input/keyboard.c src/input/scroll.c src/platform/backends/headless.c $(UI_STANDALONE_LIB) -Wl,--gc-sections -lm -o $@

build/ui_text_edit_test_sanitize build/ui_text_edit_app_test_sanitize: tests/ui_text_edit_test.c tests/ui_navigation_test.c

CONFIG_EDITOR_SRCS := examples/config_editor/document.c examples/config_editor/editor.c examples/config_editor/view.c
CONFIG_EDITOR_HEADERS := $(wildcard examples/config_editor/*.h)
.PHONY: config-editor-sdl2 config-editor-vulkan
config-editor-sdl2: build/config-editor-sdl2-$(BUILD).out
config-editor-vulkan: build/config-editor-vulkan-$(BUILD)$(PLATFORM_SUFFIX).out
build/config-editor-sdl2-$(BUILD).out: examples/config_editor/sdl_main.c $(CONFIG_EDITOR_SRCS) $(CONFIG_EDITOR_HEADERS) $(SDL_UI_LIB) $(UI_PICKER_MODULE_LIB) $(UI_STANDALONE_LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SDL_UI_FLAGS) $(filter %.c %.a,$^) $(SDL_UI_LIBS) -lm -o $@
build/config-editor-vulkan-$(BUILD)$(PLATFORM_SUFFIX).out: examples/config_editor/vulkan_main.c $(CONFIG_EDITOR_SRCS) $(CONFIG_EDITOR_HEADERS) $(LIB) shaders/vert.spv shaders/frag.spv
	$(CC) $(CPPFLAGS) $(CFLAGS) $(filter %.c %.a,$^) $(PLATFORM_LIBS) -lvulkan -lm -o $@

CONFIG_TEST_SRCS := examples/config_editor/document.c examples/config_editor/editor.c src/platform/platform_fs.c
build/config_editor_test: tests/config_editor_test.c $(CONFIG_TEST_SRCS) $(CONFIG_EDITOR_HEADERS) include/platform/platform_fs.h
	$(CC) $(CPPFLAGS) $(TEST_FLAGS) -DCONFIG_TEST_FAULT_IO $< $(CONFIG_TEST_SRCS) -Wl,--wrap=fwrite -lm -o $@
build/config_editor_test_sanitize: tests/config_editor_test.c $(CONFIG_TEST_SRCS) $(CONFIG_EDITOR_HEADERS) include/platform/platform_fs.h
	$(CC) $(CPPFLAGS) $(TEST_FLAGS) -DCONFIG_TEST_FAULT_IO -O1 -fsanitize=address,undefined -fno-omit-frame-pointer $< $(CONFIG_TEST_SRCS) -Wl,--wrap=fwrite -lm -o $@
.PHONY: test-config-editor
test-config-editor: build/config_editor_test build/config_editor_ui_test build/config_editor_session_test
	./build/config_editor_test
	./build/config_editor_ui_test
	./build/config_editor_session_test
test: test-config-editor
build/config_editor_ui_test: tests/config_editor_ui_test.c tests/ui_navigation_test.c $(CONFIG_EDITOR_SRCS) $(CONFIG_EDITOR_HEADERS) $(UI_PICKER_MODULE_LIB) src/ui/ui.c $(wildcard src/ui/*.h include/ui/*.h) $(UI_CORE_MODULE_SRCS) $(UI_STANDALONE_LIB)
	$(CC) $(CPPFLAGS) $(TEST_FLAGS) $< $(CONFIG_EDITOR_SRCS) $(UI_PICKER_MODULE_LIB) $(UI_STANDALONE_LIB) -Wl,--gc-sections -lm -o $@
build/config_editor_ui_test_sanitize: tests/config_editor_ui_test.c tests/ui_navigation_test.c $(CONFIG_EDITOR_SRCS) $(CONFIG_EDITOR_HEADERS) src/ui/ui.c $(wildcard src/ui/*.h include/ui/*.h) $(UI_CORE_MODULE_SRCS)
	$(CC) $(CPPFLAGS) $(TEST_FLAGS) -O1 -fsanitize=address,undefined -fno-omit-frame-pointer $< src/ui/ui.c $(UI_CORE_MODULE_SRCS) $(CONFIG_EDITOR_SRCS) src/ui/ui_file_picker.c src/platform/paths.c src/platform/platform_fs.c -Wl,--gc-sections -lm -o $@

.PHONY: test-config-editor-sanitize
test-config-editor-sanitize: build/config_editor_test_sanitize build/config_editor_ui_test_sanitize build/config_editor_session_test_sanitize
	ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 ./build/config_editor_test_sanitize
	ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 ./build/config_editor_ui_test_sanitize
	ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 ./build/config_editor_session_test_sanitize
test-sanitize: test-config-editor-sanitize

# Tabs/splitters exercise the actual UI core with a deterministic renderer.
build/ui_tabs_splitter_test: tests/ui_tabs_splitter_test.c tests/ui_navigation_test.c src/ui/ui.c $(wildcard src/ui/*.h include/*/*.h) $(UI_CORE_MODULE_SRCS) $(UI_STANDALONE_LIB)
	$(CC) $(CPPFLAGS) $(TEST_FLAGS) $< $(UI_STANDALONE_LIB) -Wl,--gc-sections -lm -o $@

COLOR_TEST_SRCS := src/ui/ui_file_picker.c src/platform/paths.c src/platform/platform_fs.c
build/ui_color_picker_test: tests/ui_color_picker_test.c tests/ui_navigation_test.c examples/ui_reference_scene.c examples/ui_reference_scene.h src/ui/ui.c $(COLOR_TEST_SRCS) $(wildcard src/ui/*.h include/*/*.h) $(UI_CORE_MODULE_SRCS) $(UI_STANDALONE_LIB)
	$(CC) $(CPPFLAGS) $(TEST_FLAGS) $< $(COLOR_TEST_SRCS) $(UI_STANDALONE_LIB) -Wl,--gc-sections -lm -o $@
build/ui_color_picker_test_sanitize: tests/ui_color_picker_test.c tests/ui_navigation_test.c examples/ui_reference_scene.c examples/ui_reference_scene.h src/ui/ui.c $(COLOR_TEST_SRCS) $(wildcard src/ui/*.h include/*/*.h) $(UI_CORE_MODULE_SRCS)
	$(CC) $(CPPFLAGS) $(TEST_FLAGS) -O1 -fsanitize=address,undefined -fno-omit-frame-pointer $< src/ui/ui.c $(UI_CORE_MODULE_SRCS) $(COLOR_TEST_SRCS) -Wl,--gc-sections -lm -o $@
.PHONY: test-ui-color test-ui-color-sanitize
test-ui-color: build/ui_color_picker_test
	./build/ui_color_picker_test
test-ui-color-sanitize: build/ui_color_picker_test_sanitize
	ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 ./build/ui_color_picker_test_sanitize
test: test-ui-color
test-sanitize: test-ui-color-sanitize

build/ui_basic_widgets_test: tests/ui_basic_widgets_test.c tests/ui_navigation_test.c src/ui/ui.c $(wildcard src/ui/*.h include/*/*.h) $(UI_CORE_MODULE_SRCS) $(UI_STANDALONE_LIB)
	$(CC) $(CPPFLAGS) $(TEST_FLAGS) $< $(UI_STANDALONE_LIB) -Wl,--gc-sections -lm -o $@

build/ui_interaction_test: tests/ui_interaction_test.c tests/ui_navigation_test.c src/ui/ui.c $(wildcard src/ui/*.h include/*/*.h) $(UI_CORE_MODULE_SRCS) $(UI_STANDALONE_LIB)
	$(CC) $(CPPFLAGS) $(TEST_FLAGS) $< $(UI_STANDALONE_LIB) -Wl,--gc-sections -lm -o $@

# Navigation must also work as a linked module without selecting a current context.
build/ui_navigation_module_test: tests/ui_navigation_module_test.c $(UI_STANDALONE_LIB) $(wildcard src/ui/*.h include/*/*.h)
	$(CC) $(CPPFLAGS) $(TEST_FLAGS) $< $(UI_STANDALONE_LIB) -lm -o $@

# Editing engine links without the rest of the UI; fault injection is test-only.
build/ui_text_edit_module_test: tests/ui_text_edit_module_test.c src/ui/ui_text_edit.c src/ui/ui_text_edit_internal.h $(wildcard include/*/*.h)
	$(CC) $(CPPFLAGS) $(TEST_FLAGS) $(TEXT_EDIT_FAULT_FLAGS) $< src/ui/ui_text_edit.c -lm -o $@

# Linked public consumer exercises layout, property rows and panel/scroll scopes.
build/ui_layout_modules_test: tests/ui_layout_modules_test.c $(UI_STANDALONE_LIB) $(wildcard include/ui/*.h)
	$(CC) $(CPPFLAGS) $(TEST_FLAGS) $< $(UI_STANDALONE_LIB) -lm -o $@

# Public consumer verifies capture priority across independently compiled overlays.
build/ui_overlay_modules_test: tests/ui_overlay_modules_test.c $(UI_STANDALONE_LIB) $(wildcard include/ui/*.h)
	$(CC) $(CPPFLAGS) $(TEST_FLAGS) $< $(UI_STANDALONE_LIB) -lm -o $@

# Contract test links the library and sees domain stores, never the context layout.
build/ui_state_contract_test: tests/ui_state_contract_test.c $(UI_STANDALONE_LIB) $(wildcard src/ui/ui_state_*_internal.h)
	$(CC) $(CPPFLAGS) $(TEST_FLAGS) $< $(UI_STANDALONE_LIB) -lm -o $@

# Diagnostic tool is sanitized too; it is not part of the installed SDK.
build/ui-editor-benchmark-sanitize.out: tools/ui_editor_benchmark.c tools/ui_benchmark_metrics.h tools/ui_benchmark_domains.h $(CONFIG_EDITOR_SRCS) $(CONFIG_EDITOR_HEADERS) $(UI_STANDALONE_SRCS) $(UI_PICKER_MODULE_SRCS) $(wildcard src/ui/*.h include/ui/*.h)
	$(CC) $(CPPFLAGS) $(TEST_FLAGS) -O1 -fsanitize=address,undefined -fno-omit-frame-pointer $(BENCH_ALLOC_FLAGS) tools/ui_editor_benchmark.c $(CONFIG_EDITOR_SRCS) $(UI_STANDALONE_SRCS) $(UI_PICKER_MODULE_SRCS) -lm -o $@
.PHONY: test-benchmark-sanitize
test-benchmark-sanitize: build/ui-editor-benchmark-sanitize.out
	ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 ./build/ui-editor-benchmark-sanitize.out

build/ui_dropdown_storage_test: tests/ui_dropdown_storage_test.c tests/ui_navigation_test.c $(UI_STANDALONE_LIB) $(wildcard src/ui/*.h include/ui/*.h)
	$(CC) $(CPPFLAGS) $(TEST_FLAGS) $< $(UI_STANDALONE_LIB) $(DROPDOWN_TEST_ALLOC_FLAGS) -lm -o $@

build/ui_navigation_batch_test build/ui_menu_storage_test build/ui_error_test: build/%: tests/%.c tests/ui_storage_alloc_probe.h tests/ui_navigation_test.c $(UI_STANDALONE_LIB) $(wildcard src/ui/*.h include/ui/*.h)
	$(CC) $(CPPFLAGS) $(TEST_FLAGS) $(UI_STORAGE_TEST_ALLOC_FLAGS) $< $(UI_STANDALONE_LIB) -lm -o $@
build/ui_navigation_batch_test_sanitize build/ui_menu_storage_test_sanitize: tests/ui_storage_alloc_probe.h tests/ui_navigation_test.c

build/ui_session_test: tests/ui_session_test.c tests/ui_navigation_test.c tools/ui_benchmark_metrics.h $(UI_STANDALONE_LIB) $(wildcard src/ui/*.h include/ui/*.h)
	$(CC) $(CPPFLAGS) $(TEST_FLAGS) $(BENCH_ALLOC_FLAGS) $< $(UI_STANDALONE_LIB) -lm -o $@
build/ui_session_test_sanitize: tools/ui_benchmark_metrics.h tests/ui_navigation_test.c
build/ui_error_test_sanitize: tests/ui_storage_alloc_probe.h tests/ui_navigation_test.c

# Full editor workflows with real temporary files and deterministic I/O faults.
EDITOR_SESSION_FLAGS := $(BENCH_ALLOC_FLAGS) -Wl,--wrap=fwrite
build/config_editor_session_test: tests/config_editor_session_test.c tests/ui_navigation_test.c tools/ui_benchmark_metrics.h $(CONFIG_EDITOR_SRCS) $(CONFIG_EDITOR_HEADERS) $(UI_PICKER_MODULE_LIB) $(UI_STANDALONE_LIB) $(wildcard src/ui/*.h include/ui/*.h)
	$(CC) $(CPPFLAGS) $(TEST_FLAGS) $(EDITOR_SESSION_FLAGS) $< $(CONFIG_EDITOR_SRCS) $(UI_PICKER_MODULE_LIB) $(UI_STANDALONE_LIB) -lm -o $@
build/config_editor_session_test_sanitize: tests/config_editor_session_test.c tests/ui_navigation_test.c tools/ui_benchmark_metrics.h $(CONFIG_EDITOR_SRCS) $(CONFIG_EDITOR_HEADERS) $(UI_STANDALONE_SRCS) $(UI_PICKER_MODULE_SRCS) $(wildcard src/ui/*.h include/ui/*.h)
	$(CC) $(CPPFLAGS) $(TEST_FLAGS) -O1 -fsanitize=address,undefined -fno-omit-frame-pointer $(EDITOR_SESSION_FLAGS) $< $(CONFIG_EDITOR_SRCS) $(UI_STANDALONE_SRCS) $(UI_PICKER_MODULE_SRCS) -lm -o $@

.PHONY: test-ui-release-tools
test-ui-release-tools:
	python3 tests/ui_release_tools_test.py
test: test-ui-release-tools
.PHONY: test-app-package-tools app-package
test-app-package-tools:
	python3 tests/app_package_tools_test.py
app-package:
	python3 tools/package_app.py
test: test-app-package-tools
test-ui-package: test-ui-release-tools

SDL_PLATFORM_TEST_FLAGS = $(shell pkg-config --cflags 'sdl2 >= 2.26.0')
SDL_PLATFORM_TEST_LIBS = $(shell pkg-config --libs 'sdl2 >= 2.26.0')
SDL_PLATFORM_TEST_SRCS := src/platform/backends/sdl2.c src/render/backends/sdl2_surface.c src/input/keyboard.c src/input/mouse.c src/input/scroll.c backends/sdl2/ui_sdl2_input.c
SDL_PLATFORM_WRAP := -Wl,--wrap=SDL_PollEvent,--wrap=SDL_WaitEvent,--wrap=SDL_CreateWindow,--wrap=SDL_GetWindowSizeInPixels,--wrap=realloc,--wrap=SDL_Vulkan_GetInstanceExtensions,--wrap=SDL_Vulkan_CreateSurface
build/platform_sdl2_test: tests/platform_sdl2_test.c $(SDL_PLATFORM_TEST_SRCS) $(wildcard src/platform/backends/sdl2*.h include/platform/*.h include/input/*.h)
	$(CC) $(CPPFLAGS) $(TEST_FLAGS) $(SDL_PLATFORM_TEST_FLAGS) $(filter %.c,$^) $(SDL_PLATFORM_WRAP) -Wl,--gc-sections $(SDL_PLATFORM_TEST_LIBS) -lm -o $@
build/platform_sdl2_test_sanitize: tests/platform_sdl2_test.c $(SDL_PLATFORM_TEST_SRCS) $(wildcard src/platform/backends/sdl2*.h include/platform/*.h include/input/*.h)
	$(CC) $(CPPFLAGS) $(TEST_FLAGS) $(SDL_PLATFORM_TEST_FLAGS) -O1 -fsanitize=address,undefined -fno-omit-frame-pointer $(filter %.c,$^) $(SDL_PLATFORM_WRAP) -Wl,--gc-sections $(SDL_PLATFORM_TEST_LIBS) -lm -o $@
.PHONY: test-platform-sdl2 test-platform-sdl2-sanitize
test-platform-sdl2: build/platform_sdl2_test
	SDL_VIDEODRIVER=dummy ./build/platform_sdl2_test
test-platform-sdl2-sanitize: build/platform_sdl2_test_sanitize
	SDL_VIDEODRIVER=dummy ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 ./build/platform_sdl2_test_sanitize
