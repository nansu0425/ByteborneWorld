# CMakeList.txt : CMake project for Server, include source and define
# project specific logic here.
#

# Add source to this project's executable.
add_executable (Server "Main.cpp")

# Set C++ standard
set_property(TARGET Server PROPERTY CXX_STANDARD 17)
set_property(TARGET Server PROPERTY CXX_STANDARD_REQUIRED ON)

# Enable precompiled headers using CMake's built-in support
target_precompile_headers(Server PRIVATE 
	"${CMAKE_CURRENT_SOURCE_DIR}/Pch.h"
)

# TODO: Add tests and install targets if needed.
