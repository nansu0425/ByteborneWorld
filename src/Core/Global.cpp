#include "CorePch.h"
#include "Global.h"

namespace core
{
    void testSpdlog()
    {
        spdlog::set_level(spdlog::level::debug);
        spdlog::info("Logger initialized.");
    }
}
