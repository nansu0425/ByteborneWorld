#include "Pch.h"
#include "Core/Context.h"
#include "Server.h"

int main()
{
    core::AppContext::getInstance().initialize();

    {
        WorldServer server;
        server.start();

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
       
        server.join();
    }

    core::AppContext::getInstance().cleanup();

    return 0;
}
