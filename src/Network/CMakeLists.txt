# Add source to this static library.
add_library(Network STATIC
    "Session.cpp"
    "Session.h"
)

# Set C++ standard
set_property(TARGET Network PROPERTY CXX_STANDARD 17)
set_property(TARGET Network PROPERTY CXX_STANDARD_REQUIRED ON)

# Enable precompiled headers using CMake's built-in support
target_precompile_headers(Network PRIVATE 
    "${CMAKE_CURRENT_SOURCE_DIR}/NetworkPch.h"
)

# Link libraries
target_link_libraries(Network PUBLIC
  Core
)

# Add include directories
target_include_directories(Network PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)
