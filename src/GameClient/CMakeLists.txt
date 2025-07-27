# Add source to this project's executable.
add_executable (GameClient
    "Main.cpp"
 "Pch.h")

# Enable precompiled headers using CMake's built-in support
target_precompile_headers(GameClient PRIVATE 
	"${CMAKE_CURRENT_SOURCE_DIR}/Pch.h"
)

# Link libraries
target_link_libraries(GameClient PRIVATE
    Core Network Protocol
)
