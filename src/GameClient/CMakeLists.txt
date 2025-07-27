# Add source to this project's executable.
add_executable (GameClient
    "Main.cpp"
)

# Find packages in correct order
find_package(imgui CONFIG REQUIRED)
find_package(SFML COMPONENTS graphics window system CONFIG REQUIRED)
find_package(ImGui-SFML CONFIG REQUIRED)

# Enable precompiled headers using CMake's built-in support
target_precompile_headers(GameClient PRIVATE 
    "${CMAKE_CURRENT_SOURCE_DIR}/Pch.h"
)

# Link libraries in correct order
target_link_libraries(GameClient PRIVATE
    imgui::imgui
    sfml-graphics
    sfml-window
    sfml-system
    ImGui-SFML::ImGui-SFML
    Core Network Protocol
)
