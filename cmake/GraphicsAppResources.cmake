# Deploy the installed SDK's resources beside an external executable. The copy
# runs on every build, even when only assets changed and no relink is needed.
function(graphics_app_deploy_resources target)
  if(NOT TARGET "${target}")
    message(FATAL_ERROR "graphics_app_deploy_resources: '${target}' is not a target")
  endif()
  get_target_property(_type "${target}" TYPE)
  get_target_property(_imported "${target}" IMPORTED)
  if(NOT _type STREQUAL "EXECUTABLE" OR _imported)
    message(FATAL_ERROR "graphics_app_deploy_resources requires a local executable target")
  endif()
  get_target_property(_deployed "${target}" GRAPHICS_APP_RESOURCES_DEPLOYED)
  if(_deployed)
    return()
  endif()
  if(NOT IS_DIRECTORY "${GraphicsApp_RESOURCE_DIR}")
    message(FATAL_ERROR "GraphicsApp_RESOURCE_DIR is unavailable")
  endif()
  add_custom_target(${target}_resources
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${GraphicsApp_RESOURCE_DIR}" "$<TARGET_FILE_DIR:${target}>/resources"
    VERBATIM)
  add_dependencies(${target} ${target}_resources)
  set_property(TARGET "${target}" PROPERTY GRAPHICS_APP_RESOURCES_DEPLOYED TRUE)
endfunction()
