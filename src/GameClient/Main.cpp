#include "Core/Context.h"
#include "GameClient.h"

int main()
{
    core::AppContext::getInstance().initialize();

    {
        GameClient client;
        client.start();
        client.join();
    }

    core::AppContext::getInstance().cleanup();

    return 0;
}
