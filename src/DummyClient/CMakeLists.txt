# Add source to this project's executable.
add_executable (DummyClient
    "Main.cpp"
    "Client.h" "Client.cpp"
)

# Enable precompiled headers using CMake's built-in support
target_precompile_headers(DummyClient PRIVATE 
	"${CMAKE_CURRENT_SOURCE_DIR}/Pch.h"
)

# Link libraries
target_link_libraries(DummyClient PRIVATE
    Core Network Protocol
)
