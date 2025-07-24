# Add source to this static library.
add_library(Network STATIC
    "Session.h" "Session.cpp"
    "Service.h" "Service.cpp"
    "Event.h"
    "Thread.h" "Thread.cpp"
    "Buffer.h" "Buffer.cpp"
    "Packet.h" "Packet.cpp"
)

# Enable precompiled headers using CMake's built-in support
target_precompile_headers(Network PRIVATE 
    "${CMAKE_CURRENT_SOURCE_DIR}/Pch.h"
)

# Link libraries
target_link_libraries(Network PUBLIC
    Core
)
