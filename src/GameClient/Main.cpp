#include "Core/Context.h"

int main()
{
    core::AppContext::getInstance().initialize();

    spdlog::info("[GameClient] 클라이언트 시작");
    {
        // GameClient 인스턴스 생성 및 실행
        // GameClient client;
        // client.start();
        // #ifdef _DEBUG
        // char input;
        // while (std::cin >> input)
        // {
        //     if (input == 'q' || input == 'Q')
        //     {
        //         ::raise(SIGINT);
        //         break;
        //     }
        // }
        // #endif // _DEBUG
        // client.join();
    }

    core::AppContext::getInstance().cleanup();

    return 0;
}
