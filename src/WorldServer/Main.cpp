#include "Pch.h"
#include "Core/Context.h"
#include "Server.h"

int main()
{
    core::AppContext::getInstance().initialize();

    WorldServer server;
    server.start();

    // q 입력 시 서버 중지
    char input;
    while ((input = ::getchar()) && (input != 'q'))
    {}
    server.stop();
    server.watiForStop();

    core::AppContext::getInstance().cleanup();

    return 0;
}
