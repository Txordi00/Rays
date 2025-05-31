file(GLOB SRC_src CONFIGURE_DEPENDS "*.hpp" "*.cpp")
set_property(SOURCE ${SRC_src} PROPERTY HEADER_FILE_ONLY ON)
target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${SRC_src})
message("Source files in ${CMAKE_CURRENT_SOURCE_DIR}: ${SRC_src}")
