cmake_minimum_required(VERSION 3.16)
project(serial_vst C)

set(CMAKE_C_STANDARD 11)

# Build as shared library (.dll)
add_library(serial_vst SHARED serial_vst.c)

target_include_directories(serial_vst PRIVATE sdk)

target_link_libraries(serial_vst
    ole32
    winmm
)

target_compile_options(serial_vst PRIVATE -O2 -Wall)

# Export VSTPluginMain and main alias via .def file
target_link_options(serial_vst PRIVATE -Wl,--kill-at ${CMAKE_CURRENT_SOURCE_DIR}/serial_vst.def)

# Output filename without "lib" prefix
set_target_properties(serial_vst PROPERTIES
    PREFIX ""
    OUTPUT_NAME "serial_vst"
)