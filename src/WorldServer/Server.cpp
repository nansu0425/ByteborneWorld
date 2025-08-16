#include "Server.h"
#include "Network/Packet.h"
#include "Protocol/Type.h"
#include "Protocol/Serializer.h"
#include <chrono>

WorldServer::WorldServer()
    : m_running(false)
    , m_messageSerializer(m_sendBufferManager)
    , m_chatRoom(m_sessionManager, m_messageSerializer)
{
    m_serverService = net::ServerService::createInstance(
        m_ioThreadPool.getContext(), m_serviceEventQueue, 12345);

    registerMessageHandlers();
}

void WorldServer::start()
{
    if (m_running.exchange(true))
    {
        // 이미 실행 중인 경우 아무 작업도 하지 않음
        return;
    }

    spdlog::info("[WorldServer] 서버 시작");

    m_mainThread = std::thread(
        [this]()
        {
            loop();
            close();
        });
    m_ioThreadPool.run();
    
    m_serverService->start();
}

void WorldServer::stop()
{
    if (!m_running.exchange(false))
    {
        // 이미 중지 상태이므로 아무 작업도 하지 않음
        return;
    }

    spdlog::info("[WorldServer] 서버 중지");
}

void WorldServer::join()
{
    m_ioThreadPool.join();
    if (m_mainThread.joinable())
    {
        m_mainThread.join();
    }

    spdlog::info("[WorldServer] 서버 종료");
}

void WorldServer::loop()
{
    constexpr auto TickInterval = std::chrono::milliseconds(50);
    auto lastTickCountTime = std::chrono::steady_clock::now();
    int32_t tickCount = 0;

    while (m_running.load())
    {
        auto start = std::chrono::steady_clock::now();

        processServiceEvents();
        processSessionEvents();
        processMessages();
        m_timer.update();
        ++tickCount;

        auto end = std::chrono::steady_clock::now();

        // 1초마다 틱 카운트 로그 출력
        auto tickCountElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - lastTickCountTime);
        if (std::chrono::seconds(1) <= tickCountElapsed)
        {
            spdlog::debug("[WorldServer] 틱 카운트: {}", tickCount);
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

void WorldServer::close()
{
    // 실행 중이 아닌 상태에서 close가 호출돼야 한다
    assert(m_running.load() == false);

    spdlog::info("[WorldServer] 서버 닫기");

    m_sessionManager.stopAllSessions();

    // 서모든 세션이 제거될 때까지 대기
    while (m_sessionManager.isEmpty() == false)
    {
        processSessionEvents();
        std::this_thread::yield();
    }

    // io 스레드가 처리할 핸들러가 없으면 스레드가 종료되도록 설정
    m_ioThreadPool.reset();
}

void WorldServer::processServiceEvents()
{
    net::ServiceEventPtr event;
    while (m_serviceEventQueue.pop(event))
    {
        switch (event->type)
        {
        case net::ServiceEventType::Close:
            handleServiceEvent(*static_cast<net::ServiceCloseEvent*>(event.get()));
            break;
        case net::ServiceEventType::Accept:
            handleServiceEvent(*static_cast<net::ServiceAcceptEvent*>(event.get()));
            break;
        default:
            spdlog::error("[WorldServer] 알 수 없는 서비스 이벤트 타입: {}", static_cast<int>(event->type));
            assert(false);
            break;
        }
    }
}

void WorldServer::handleServiceEvent(net::ServiceCloseEvent& event)
{
    assert(m_running.load());

    stop();
}

void WorldServer::handleServiceEvent(net::ServiceAcceptEvent& event)
{
    if (!m_running.load())
    {
        spdlog::debug("[WorldServer] 서버가 실행 중이 아닙니다. 클라이언트 수락 이벤트를 건너뜁니다.");
        return;
    }

    auto session = net::Session::createInstance(std::move(event.socket), m_sessionEventQueue);
    m_sessionManager.addSession(session);
    m_chatRoom.onClientAccepted(session->getSessionId());
    session->start();
}

void WorldServer::processSessionEvents()
{
    net::SessionEventPtr event;
    while (m_sessionEventQueue.pop(event))
    {
        switch (event->type)
        {
        case net::SessionEventType::Close:
            handleSessionEvent(*static_cast<net::SessionCloseEvent*>(event.get()));
            break;
        case net::SessionEventType::Receive:
            handleSessionEvent(*static_cast<net::SessionReceiveEvent*>(event.get()));
            break;
        default:
            spdlog::error("[WorldServer] 알 수 없는 세션 이벤트 타입: {}", static_cast<int>(event->type));
            assert(false);
            break;
        }
    }
}

void WorldServer::handleSessionEvent(net::SessionCloseEvent& event)
{
    m_sessionManager.removeSession(event.sessionId);
    m_chatRoom.onClientClosed(event.sessionId);
}

void WorldServer::handleSessionEvent(net::SessionReceiveEvent& event)
{
    if (!m_running.load())
    {
        spdlog::debug("[WorldServer] 서비스가 실행 중이 아닙니다. 수신 이벤트를 건너뜁니다.");
        return;
    }

    auto session = m_sessionManager.findSession(event.sessionId);
    assert(session);

    net::PacketView packetView;
    while (session->getFrontPacket(packetView))
    {
        // 패킷의 페이로드를 메시지로 파싱하여 큐에 추가
        m_messageQueue.push(event.sessionId, packetView);

        // 수신 버퍼에서 패킷 제거
        session->popFrontPacket();
    }

    // 세션에서 다시 비동기 수신 시작
    session->receive();
}

void WorldServer::processMessages()
{
    while (m_running.load() && (m_messageQueue.isEmpty() == false))
    {
        m_messageDispatcher.dispatch(m_messageQueue.front());
        m_messageQueue.pop();
    }
}

void WorldServer::registerMessageHandlers()
{
    // 채팅 핸들러를 ChatRoom에 위임
    m_chatRoom.registerMessageHandlers(m_messageDispatcher);
}
