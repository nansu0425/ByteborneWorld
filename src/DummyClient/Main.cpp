#include "Pch.h"
#include "Core/Context.h"

int main()
{
    core::AppContext::getInstance().initialize();

    SPDLOG_INFO("[DummyClient] Dummy Client 시작");

    core::AppContext::getInstance().cleanup();

    return 0;
}
