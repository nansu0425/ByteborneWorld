#include "Pch.h"
#include "Client.h"

DummyClient::DummyClient()
    : m_running(false)
{
    m_clientService = net::ClientService::createInstance(
        m_ioThreadPool.getContext(), net::ResolveTarget{"localhost", "12345"});
}

void DummyClient::start()
{
    if (m_running.exchange(true))
    {
        // 이미 실행 중인 경우 아무 작업도 하지 않음
        return;
    }

    SPDLOG_INFO("[DummyClient] 클라이언트 시작");

    m_mainThread = std::thread(
        [this]()
        {
            try
            {
                loop();
                close();
            }
            catch (const std::exception& e)
            {
                SPDLOG_ERROR("[DummyClient] 메인 스레드 오류: {}", e.what());
            }
        });
    m_ioThreadPool.run();
    
    m_clientService->start();
}

void DummyClient::stop()
{
    if (!m_running.exchange(false))
    {
        // 이미 중지 상태이므로 아무 작업도 하지 않음
        return;
    }

    SPDLOG_INFO("[DummyClient] 클라이언트 중지");
}

void DummyClient::join()
{
    m_ioThreadPool.join();
    if (m_mainThread.joinable())
    {
        m_mainThread.join();
    }

    SPDLOG_INFO("[DummyClient] 클라이언트 종료");
}

void DummyClient::loop()
{
    constexpr auto TickInterval = std::chrono::milliseconds(50);
    auto lastTickCountTime = std::chrono::steady_clock::now();
    int32_t tickCount = 0;

    while (m_running.load())
    {
        auto start = std::chrono::steady_clock::now();

        processServiceEvents();
        processSessionEvents();
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

void DummyClient::close()
{
    SPDLOG_INFO("[DummyClient] 클라이언트 닫기");

    m_sessionManager.stopAllSessions();

    // 서비스 이벤트 큐가 비워지고, 모든 세션이 제거될 때까지 대기
    while ((!m_clientService->isEventQueueEmpty()) ||
           (!m_sessionManager.isEmpty()))
    {
        processServiceEvents();
        processSessionEvents();

        std::this_thread::yield();
    }

    // io 스레드가 처리할 핸들러가 없으면 스레드가 종료되도록 설정
    m_ioThreadPool.reset();
}

void DummyClient::processServiceEvents()
{
    while (auto event = m_clientService->popEvent())
    {
        switch (event->type)
        {
        case net::ServiceEventType::Close:
            handleServiceEvent(*static_cast<net::CloseServiceEvent*>(event.get()));
            break;
        case net::ServiceEventType::Connect:
            handleServiceEvent(*static_cast<net::ConnectServiceEvent*>(event.get()));
            break;
        default:
            SPDLOG_WARN("[DummyClient] 알 수 없는 서비스 이벤트 타입: {}", static_cast<int>(event->type));
            assert(false);
            break;
        }
    }
}

void DummyClient::handleServiceEvent(net::CloseServiceEvent& event)
{
    assert(m_running.load());

    SPDLOG_INFO("[DummyClient] 서비스 닫기 이벤트 처리");

    stop();
}

void DummyClient::handleServiceEvent(net::ConnectServiceEvent& event)
{
    if (!m_running.load())
    {
        SPDLOG_WARN("[DummyClient] 클라이언트가 실행 중이 아닙니다. 서버 연결 이벤트를 건너뜁니다.");
        return;
    }

    SPDLOG_INFO("[DummyClient] 서버 연결 이벤트 처리");

    auto session = net::Session::createInstance(std::move(event.socket), m_sessionEventQueue);
    m_sessionManager.addSession(session);
    session->start();
}

void DummyClient::processSessionEvents()
{
    while (auto event = m_sessionEventQueue.pop())
    {
        switch (event->type)
        {
        case net::SessionEventType::Close:
            handleSessionEvent(*static_cast<net::CloseSessionEvent*>(event.get()));
            break;
        case net::SessionEventType::Receive:
            handleSessionEvent(*static_cast<net::ReceiveSessionEvent*>(event.get()));
            break;
        default:
            SPDLOG_WARN("[DummyClient] 알 수 없는 세션 이벤트 타입: {}", static_cast<int>(event->type));
            assert(false);
            break;
        }
    }
}

void DummyClient::handleSessionEvent(net::CloseSessionEvent& event)
{
    SPDLOG_INFO("[DummyClient] 세션 닫기 이벤트 처리: {}", event.sessionId);

    m_sessionManager.removeSession(event.sessionId);
}

void DummyClient::handleSessionEvent(net::ReceiveSessionEvent& event)
{
    if (!m_running.load())
    {
        SPDLOG_WARN("[DummyClient] 클라이언트가 실행 중이 아닙니다. 수신 이벤트를 건너뜁니다.");
        return;
    }

    SPDLOG_INFO("[DummyClient] 수신 이벤트 처리: {}", event.sessionId);
}
