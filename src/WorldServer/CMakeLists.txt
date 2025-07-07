# Add source to this project's executable.
add_executable (WorldServer
    "Main.cpp"
    "Server.h" "Server.cpp"
)

# Set C++ standard
set_property(TARGET WorldServer PROPERTY CXX_STANDARD 17)
set_property(TARGET WorldServer PROPERTY CXX_STANDARD_REQUIRED ON)

# Enable precompiled headers using CMake's built-in support
target_precompile_headers(WorldServer PRIVATE 
	"${CMAKE_CURRENT_SOURCE_DIR}/Pch.h"
)

# Link libraries
target_link_libraries(WorldServer PRIVATE
  Core Network
)
