#include "Pch.h"

int main()
{
    std::cout << "Hello CMake with Precompiled Headers!" << std::endl;
    
    // Example using other headers from PCH
    std::vector<std::string> messages = {
        "Precompiled headers are working!",
        "Compilation should be faster now."
    };
    
    for (const auto& message : messages) {
        std::cout << message << std::endl;
    }
    
    return 0;
}
