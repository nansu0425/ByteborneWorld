# Find and link libraries
find_package(spdlog CONFIG REQUIRED)

# Add source to this static library.
add_library(Core STATIC
    "Context.h" "Context.cpp"
)

# Set C++ standard
set_property(TARGET Core PROPERTY CXX_STANDARD 17)
set_property(TARGET Core PROPERTY CXX_STANDARD_REQUIRED ON)

# Enable precompiled headers using CMake's built-in support
target_precompile_headers(Core PRIVATE 
    "${CMAKE_CURRENT_SOURCE_DIR}/Pch.h"
)

# Link libraries
target_link_libraries(Core PUBLIC
    spdlog::spdlog
)

# Add include directories
target_include_directories(Core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
)
