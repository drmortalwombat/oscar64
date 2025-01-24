cmake_minimum_required(VERSION 3.15)

# Project name and version
project(oscar64 VERSION 1.0 LANGUAGES CXX)

# Define the executable
add_executable(oscar64)


# Glob all source files in the oscar64/ directory
file(GLOB OSCAR64_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/oscar64/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/oscar64/*.h")

# Add the sources to the target
target_sources(${PROJECT_NAME} PRIVATE ${OSCAR64_SOURCES})


# Add header files
target_include_directories(oscar64 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Compiler settings based on configuration
set_target_properties(oscar64 PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

# Set debug and release configurations
target_compile_definitions(oscar64 PRIVATE $<$<CONFIG:Debug>:_DEBUG;_CONSOLE> $<$<CONFIG:Release>:NDEBUG;_CONSOLE>)
target_compile_options(oscar64 PRIVATE $<$<CONFIG:Debug>:/W3 /WX> $<$<CONFIG:Release>:/O2 /W3>)

# Post-build steps (only for Release builds)
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_custom_command(TARGET oscar64 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:oscar64> ${CMAKE_BINARY_DIR}/bin
    )
endif()

if (WIN32)
    target_link_libraries(oscar64 PRIVATE kernel32 user32 gdi32 winspool comdlg32 advapi32 shell32 ole32 oleaut32 uuid odbc32 odbccp32 version)
elseif (APPLE)
    # Add macOS-specific frameworks to replace Windows libraries
    target_link_libraries(${PROJECT_NAME} 
        "-framework Cocoa"        # For GUI and application support
        "-framework CoreFoundation" # For Core Foundation utilities (similar to advapi32 or shell32)
        "-framework CoreGraphics"  # For graphics and rendering (similar to gdi32)
        "-framework IOKit"         # For hardware interaction
        "-framework AppKit"        # For GUI (equivalent to user32)
    )
elseif (UNIX)
    # Add Linux-specific libraries for equivalent functionality
    target_link_libraries(${PROJECT_NAME} 
        pthread                    # For threading (similar to kernel32)
        dl                         # For dynamic library loading (similar to LoadLibrary)
        m                          # For math library (optional but common on Linux)
        X11                        # For X Window System (similar to user32 for GUI support)
    )
endif()


# Output directories
set_target_properties(oscar64 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/Release
)

