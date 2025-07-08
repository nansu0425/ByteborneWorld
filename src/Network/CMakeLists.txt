# Add source to this static library.
add_library(Network STATIC
    "Session.h" "Session.cpp"
    "Service.h" "Service.cpp"
    "Queue.h" "Queue.cpp"
    "Event.h"
    "Thread.h" "Thread.cpp"
)

# Set C++ standard
set_property(TARGET Network PROPERTY CXX_STANDARD 17)
set_property(TARGET Network PROPERTY CXX_STANDARD_REQUIRED ON)

# Enable precompiled headers using CMake's built-in support
target_precompile_headers(Network PRIVATE 
    "${CMAKE_CURRENT_SOURCE_DIR}/Pch.h"
)

# Link libraries
target_link_libraries(Network PUBLIC
    Core
)
