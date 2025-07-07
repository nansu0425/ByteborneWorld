#include "Pch.h"
#include "Context.h"
#include "Server.h"

int main()
{
    core::AppContext::getInstance().initialize();

    WorldServer server;
    server.run();
    server.join();

    core::AppContext::getInstance().cleanup();

    return 0;
}
