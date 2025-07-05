#include "Pch.h"

int main()
{
    core::AppContext::getInstance().initialize();

    SPDLOG_INFO("월드 서버 시작");
    SPDLOG_WARN("이것은 경고 메시지입니다.");
    SPDLOG_ERROR("이것은 오류 메시지입니다.");

    core::AppContext::getInstance().cleanup();

    return 0;
}
