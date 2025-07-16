#include "Pch.h"
#include "Client.h"

DummyClient::DummyClient()
    : m_running(false)
{
    m_clientService = net::ClientService::createInstance(
        m_ioThreadPool.getContext(), net::ResolveTarget{"localhost", "12345"}, 5);
}

void DummyClient::start()
{
    if (m_running.exchange(true))
    {
        // 이미 실행 중인 경우 아무 작업도 하지 않음
        return;
    }

    spdlog::info("[DummyClient] 클라이언트 시작");

    m_mainThread = std::thread(
        [this]()
        {
            loop();
            close();
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

    spdlog::info("[DummyClient] 클라이언트 중지");
}

void DummyClient::join()
{
    m_ioThreadPool.join();
    if (m_mainThread.joinable())
    {
        m_mainThread.join();
    }

    spdlog::info("[DummyClient] 클라이언트 종료");
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
            spdlog::debug("[DummyClient] 틱 카운트: {}", tickCount);
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
    // 실행 중이 아닌 상태에서 close가 호출돼야 한다
    assert(m_running.load() == false);

    spdlog::info("[DummyClient] 클라이언트 닫기");

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
            spdlog::error("[DummyClient] 알 수 없는 서비스 이벤트 타입: {}", static_cast<int>(event->type));
            assert(false);
            break;
        }
    }
}

void DummyClient::handleServiceEvent(net::CloseServiceEvent& event)
{
    assert(m_running.load());

    stop();

    spdlog::debug("[DummyClient] 서비스 닫기 이벤트 처리");
}

void DummyClient::handleServiceEvent(net::ConnectServiceEvent& event)
{
    if (!m_running.load())
    {
        spdlog::debug("[DummyClient] 클라이언트가 실행 중이 아닙니다. 서버 연결 이벤트를 건너뜁니다.");
        return;
    }

    auto session = net::Session::createInstance(std::move(event.socket), m_sessionEventQueue);
    m_sessionManager.addSession(session);
    session->start();

    spdlog::debug("[DummyClient] 서버 연결 이벤트 처리");
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
            spdlog::error("[DummyClient] 알 수 없는 세션 이벤트 타입: {}", static_cast<int>(event->type));
            assert(false);
            break;
        }
    }
}

void DummyClient::handleSessionEvent(net::CloseSessionEvent& event)
{
    m_sessionManager.removeSession(event.sessionId);

    spdlog::debug("[DummyClient] 세션 닫기 이벤트 처리: {}", event.sessionId);
}

void DummyClient::handleSessionEvent(net::ReceiveSessionEvent& event)
{
    if (!m_running.load())
    {
        spdlog::debug("[DummyClient] 클라이언트가 실행 중이 아닙니다. 수신 이벤트를 건너뜁니다.");
        return;
    }

    auto session = m_sessionManager.findSession(event.sessionId);
    if (!session)
    {
        spdlog::error("[DummyClient] 세션을 찾을 수 없습니다: {}", event.sessionId);
        assert(false);
        return;
    }

    auto& receiveBuffer = session->getReceiveBuffer();
    spdlog::debug("[DummyClient] 세션 {}에서 수신된 데이터 크기: {}", event.sessionId, receiveBuffer.getDataSize());

    // TODO: 실제 수신 데이터 처리 로직 구현

    receiveBuffer.onRead(receiveBuffer.getDataSize());

    // 세션에서 다시 비동기 수신 시작
    session->receive();

    spdlog::debug("[DummyClient] 수신 이벤트 처리: {}", event.sessionId);
}
