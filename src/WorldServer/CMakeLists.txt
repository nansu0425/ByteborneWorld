# Add source to this project's executable.
add_executable (WorldServer
    "Main.cpp"
    "Server.h" "Server.cpp"
)

# Enable precompiled headers using CMake's built-in support
target_precompile_headers(WorldServer PRIVATE 
	"${CMAKE_CURRENT_SOURCE_DIR}/Pch.h"
)

# Link libraries
target_link_libraries(WorldServer PRIVATE
    Core Network Protocol
)
