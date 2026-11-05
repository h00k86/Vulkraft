# Optional complete application SDK. UIFramework alone has no Vulkan/host dependency.
set(GRAPHICS_APP_VERSION 0.7.0)
set(GRAPHICS_APP_PLATFORM "glfw" CACHE STRING "GraphicsApp window host: glfw or sdl2")
set_property(CACHE GRAPHICS_APP_PLATFORM PROPERTY STRINGS glfw sdl2)
if(NOT GRAPHICS_APP_PLATFORM MATCHES "^(glfw|sdl2)$")
  message(FATAL_ERROR "GRAPHICS_APP_PLATFORM must be glfw or sdl2")
endif()
option(GRAPHICS_APP_BUILD_EXAMPLE "Build the complete starter application" ON)
find_package(Vulkan 1.3 REQUIRED)
if(GRAPHICS_APP_PLATFORM STREQUAL "glfw")
  find_package(glfw3 3.3 REQUIRED)
  set(_graphics_app_host glfw)
else()
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(GRAPHICS_APP_SDL2 REQUIRED IMPORTED_TARGET sdl2>=2.26.0)
  set(_graphics_app_host PkgConfig::GRAPHICS_APP_SDL2)
endif()
add_library(graphics_app STATIC
  src/app/application.c src/app/application_legacy.c src/app/resources.c
  src/input/keyboard.c src/input/mouse.c src/input/scroll.c
  src/math/matrix.c src/platform/window.c
  src/platform/backends/${GRAPHICS_APP_PLATFORM}.c
  src/render/backends/${GRAPHICS_APP_PLATFORM}_surface.c
  src/ui/ui_app_adapter.c
  src/render/font.c src/render/frame_dump.c src/render/gfx.c src/render/init_vulkan.c
  src/render/stb_image_impl.c src/render/text_renderer.c src/render/texture_manager.c
  src/render/ui_renderer.c src/render/vsync_settings.c
  src/render/vulkan_buffer.c src/render/vulkan_command.c src/render/vulkan_descriptors.c
  src/render/vulkan_draw.c src/render/vulkan_framebuffer.c src/render/vulkan_instance.c
  src/render/vulkan_logical_device.c
  src/render/vulkan_physical_device.c src/render/vulkan_pipeline.c
  src/render/vulkan_renderpass.c src/render/vulkan_shader.c src/render/vulkan_surface.c
  src/render/vulkan_swapchain.c src/render/vulkan_sync.c src/render/vulkan_texture.c
  src/render/vulkan_uniform_buffer.c src/render/vulkan_vertex_buffer.c)
add_library(GraphicsApp::application ALIAS graphics_app)
set_target_properties(graphics_app PROPERTIES EXPORT_NAME application)
target_link_libraries(graphics_app PUBLIC UIFramework::picker
  PRIVATE Vulkan::Vulkan ${_graphics_app_host})
target_compile_features(graphics_app PUBLIC c_std_11)
target_compile_definitions(graphics_app PRIVATE $<$<CONFIG:Debug>:DEBUG>)
if(NOT MSVC)
  target_compile_options(graphics_app PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

# Compile the canonical GLSL sources; never rely on checked-in SPIR-V freshness.
if(NOT Vulkan_GLSLC_EXECUTABLE)
  find_program(Vulkan_GLSLC_EXECUTABLE NAMES glslc REQUIRED)
endif()
foreach(shader vert frag)
  set(output "${CMAKE_CURRENT_BINARY_DIR}/graphics-app-resources/shaders/${shader}.spv")
  add_custom_command(OUTPUT "${output}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/graphics-app-resources/shaders"
    COMMAND "${Vulkan_GLSLC_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/tri.${shader}" -o "${output}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/tri.${shader}" VERBATIM)
  list(APPEND _graphics_app_shaders "${output}")
endforeach()
add_custom_target(graphics_app_shaders ALL DEPENDS ${_graphics_app_shaders})
add_dependencies(graphics_app graphics_app_shaders)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/graphics-app-resources/assets/fonts")
foreach(font font_utf8.json font_utf8.png)
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/${font}"
    "${CMAKE_CURRENT_BINARY_DIR}/graphics-app-resources/assets/fonts/${font}" COPYONLY)
endforeach()
if(GRAPHICS_APP_BUILD_EXAMPLE)
  add_executable(graphics_app_starter examples/external_app/main.c)
  target_link_libraries(graphics_app_starter PRIVATE GraphicsApp::application)
  add_custom_target(graphics_app_starter_resources ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${CMAKE_CURRENT_BINARY_DIR}/graphics-app-resources" "$<TARGET_FILE_DIR:graphics_app_starter>/resources"
    DEPENDS graphics_app_shaders VERBATIM)
  add_dependencies(graphics_app_starter graphics_app_starter_resources)
endif()
if(BUILD_TESTING)
  add_executable(app_resources_test tests/app_resources_test.c src/app/resources.c)
  target_link_libraries(app_resources_test PRIVATE ui_picker)
  add_test(NAME app_resources COMMAND app_resources_test)
  add_executable(app_callbacks_test tests/app_loop_failure_test.c)
  target_include_directories(app_callbacks_test PRIVATE include)
  if(NOT MSVC)
    target_compile_options(app_callbacks_test PRIVATE -Wall -Wextra -Wno-unused-parameter)
  endif()
  add_test(NAME app_callbacks COMMAND app_callbacks_test)
endif()

install(TARGETS graphics_app EXPORT GraphicsAppTargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(EXPORT GraphicsAppTargets NAMESPACE GraphicsApp:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/GraphicsApp)
install(DIRECTORY include/app/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/app)
install(FILES include/math/matrix.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/math)
install(FILES include/render/gfx.h include/render/font.h include/render/text_anchor.h
  include/render/text_renderer.h include/render/vsync_settings.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/render)
install(FILES include/input/keyboard.h include/input/mouse.h include/input/scroll.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/input)
install(FILES include/platform/paths.h include/platform/window.h include/platform/events.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/platform)
install(FILES include/ui/app_adapter.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ui)
install(FILES assets/fonts/font_utf8.json assets/fonts/font_utf8.png assets/fonts/OpenSans-OFL.txt
  DESTINATION ${CMAKE_INSTALL_DATADIR}/graphics-app/resources/assets/fonts)
install(FILES ${_graphics_app_shaders} DESTINATION ${CMAKE_INSTALL_DATADIR}/graphics-app/resources/shaders)
install(DIRECTORY examples/external_app/ DESTINATION ${CMAKE_INSTALL_DATADIR}/graphics-app/examples/external_app)
install(FILES examples/config_editor/CMakeLists.txt examples/config_editor/vulkan_main.c
  examples/config_editor/document.c examples/config_editor/document.h
  examples/config_editor/editor.c examples/config_editor/editor.h
  examples/config_editor/view.c examples/config_editor/view.h examples/config_editor/example.cfgapp
  DESTINATION ${CMAKE_INSTALL_DATADIR}/graphics-app/examples/config_editor)
install(FILES docs/GRAPHICS_APP_SDK.md DESTINATION ${CMAKE_INSTALL_DATADIR}/graphics-app)
install(FILES docs/GRAPHICS_APP_API.md docs/UI_INTEGRATION_BEHAVIOR.md docs/CONFIG_EDITOR_GUIDE.md docs/CONFIG_EDITOR.md DESTINATION ${CMAKE_INSTALL_DATADIR}/graphics-app)
configure_package_config_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/GraphicsAppConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/GraphicsAppConfig.cmake"
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/GraphicsApp
  PATH_VARS CMAKE_INSTALL_DATADIR)
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/GraphicsAppConfigVersion.cmake"
  VERSION ${GRAPHICS_APP_VERSION} COMPATIBILITY ExactVersion)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/GraphicsAppConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/GraphicsAppConfigVersion.cmake"
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/GraphicsAppResources.cmake"
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/GraphicsApp)
unset(_graphics_app_host)
unset(_graphics_app_shaders)
