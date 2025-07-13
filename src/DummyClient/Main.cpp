#include "Pch.h"
#include "Core/Context.h"
#include "Client.h"

int main()
{
    core::AppContext::getInstance().initialize();

    {
        DummyClient client;
        client.start();

#ifdef _DEBUG
        char input;
        while (std::cin >> input)
        {
            if (input == 'q' || input == 'Q')
            {
                ::raise(SIGINT);
                break;
            }
        }
#endif // _DEBUG

        client.join();
    }

    core::AppContext::getInstance().cleanup();

    return 0;
}
