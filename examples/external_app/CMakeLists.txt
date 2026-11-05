cmake_minimum_required(VERSION 3.20)
project(ExternalGraphicsApplication LANGUAGES C)
find_package(GraphicsApp 0.5.1 EXACT REQUIRED)
message(STATUS "GraphicsApp host: ${GraphicsApp_PLATFORM}")
add_executable(external_app main.c)
target_link_libraries(external_app PRIVATE GraphicsApp::application)
# Deploy resources beside the consumer. Its executable locates them at runtime;
# no absolute SDK/repository path is embedded into the application.
graphics_app_deploy_resources(external_app)
