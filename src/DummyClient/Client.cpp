#include "Pch.h"
#include "Client.h"

DummyClient::DummyClient()
    : m_running(false)
{}

void DummyClient::start()
{
    m_running.store(true);
    SPDLOG_INFO("[DummyClient] 클라이언트 시작");

    m_clientIoService = net::ClientIoService::createInstance(m_ioEventQueue, net::ResolveTarget{"localhost", "12345"});
    m_clientIoService->start();
    m_clientIoService->asyncWaitForStopSignals(
        [this](const asio::error_code& error, int signalNumber)
        {
            if (!error)
            {
                stop();
            }
            else
            {
                SPDLOG_ERROR("[DummyClient] 중지 시그널 처리 오류: {}", error.value());
            }
        });

    m_mainLoopThread = std::thread(
        [this]()
        {
            try
            {
                runMainLoop();
            }
            catch (const std::exception& e)
            {
                SPDLOG_ERROR("[DummyClient] 루프 스레드 오류: {}", e.what());
            }
        });
}

void DummyClient::stop()
{
    if (m_running.exchange(false))
    {
        SPDLOG_INFO("[DummyClient] 클라이언트 중지");

        m_clientIoService->stop();
    }
}

void DummyClient::watiForStop()
{
    if (m_mainLoopThread.joinable())
    {
        m_mainLoopThread.join();
    }
    m_clientIoService->waitForStop();

    SPDLOG_INFO("[DummyClient] 클라이언트 종료");
}

void DummyClient::runMainLoop()
{
    constexpr auto TickInterval = std::chrono::milliseconds(50);
    auto lastTickCountTime = std::chrono::steady_clock::now();
    int32_t tickCount = 0;

    while (m_running.load())
    {
        auto start = std::chrono::steady_clock::now();

        processIoEvents();
        ++tickCount;

        auto end = std::chrono::steady_clock::now();

        // 1초마다 틱 카운트 로그 출력
        auto tickCountElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - lastTickCountTime);
        if (std::chrono::seconds(1) <= tickCountElapsed)
        {
            SPDLOG_INFO("[DummyClient] 틱 카운트: {}", tickCount);
            lastTickCountTime = end;
            tickCount = 0;
        }

        // 다음 틱까지 대기
        std::chrono::milliseconds elapsed;
        do
        {
            auto now = std::chrono::steady_clock::now();
            elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start);
        }
        while (elapsed < TickInterval);
    }
}

void DummyClient::processIoEvents()
{
    while (!m_ioEventQueue.isEmpty())
    {
        auto event = m_ioEventQueue.pop();
        if (event)
        {
            switch (event->type)
            {
            case net::IoEventType::Connect:
                handleConnectEvent(event->session);
                break;
            case net::IoEventType::Disconnect:
                handleDisconnectEvent(event->session);
                break;
            case net::IoEventType::Receive:
                handleRecevieEvent(event->session);
                break;
            }
        }
    }
}

void DummyClient::handleConnectEvent(const net::SessionPtr& session)
{
    m_sessionManager.addSession(session);
}

void DummyClient::handleDisconnectEvent(const net::SessionPtr& session)
{
    m_sessionManager.removeSession(session);
}

void DummyClient::handleRecevieEvent(const net::SessionPtr& session)
{
    // TODO: 수신 데이터 처리 로직 추가

    // 다음 수신 요청
    session->receive();
}
