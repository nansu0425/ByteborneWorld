#include "Pch.h"
#include "Core/Context.h"
#include "Client.h"

int main()
{
    core::AppContext::getInstance().initialize();

    DummyClient client;
    client.start();

    char input;
    while (std::cin >> input)
    {
        if (input == 'q' || input == 'Q')
        {
            break;
        }
    }

    client.stop();
    client.watiForStop();

    core::AppContext::getInstance().cleanup();

    return 0;
}
