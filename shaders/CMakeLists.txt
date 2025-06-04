# Set command to compile shaders into the binary location
add_custom_target(shaders)
add_custom_command(TARGET shaders PRE_BUILD COMMAND ${CMAKE_SOURCE_DIR}/compile_shaders.sh)

file(GLOB SRC_shaders CONFIGURE_DEPENDS "*.frag" "*.vert" "*.comp")
target_sources(shaders PRIVATE ${SRC_shaders})
message("Source files in ${CMAKE_CURRENT_SOURCE_DIR}: ${SRC_shaders}")
