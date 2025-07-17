#include "Pch.h"
#include "Server.h"
#include "Network/Packet.h"

WorldServer::WorldServer()
    : m_running(false)
{
    m_serverService = net::ServerService::createInstance(
        m_ioThreadPool.getContext(), 12345);
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

    std::string sendMessage = "Hello from WorldServer!";
    std::vector<uint8_t> packet(sizeof(net::PacketHeader) + sendMessage.size());
    net::PacketHeader* header = reinterpret_cast<net::PacketHeader*>(packet.data());
    header->size = static_cast<net::PacketSize>(packet.size());
    header->id = 1000;
    std::memcpy(header + 1, sendMessage.c_str(), sendMessage.size());

    while (m_running.load())
    {
        auto start = std::chrono::steady_clock::now();

        processServiceEvents();
        processSessionEvents();
        m_sessionManager.broadcast(packet);
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

    // 서비스 이벤트 큐가 비워지고, 모든 세션이 제거될 때까지 대기
    while ((!m_serverService->isEventQueueEmpty()) ||
           (!m_sessionManager.isEmpty()))
    {
        processServiceEvents();
        processSessionEvents();

        std::this_thread::yield();
    }

    // io 스레드가 처리할 핸들러가 없으면 스레드가 종료되도록 설정
    m_ioThreadPool.reset();
}

void WorldServer::processServiceEvents()
{
    while (auto event = m_serverService->popEvent())
    {
        switch (event->type)
        {
        case net::ServiceEventType::Close:
            handleServiceEvent(*static_cast<net::CloseServiceEvent*>(event.get()));
            break;
        case net::ServiceEventType::Accept:
            handleServiceEvent(*static_cast<net::AcceptServiceEvent*>(event.get()));
            break;
        default:
            spdlog::error("[WorldServer] 알 수 없는 서비스 이벤트 타입: {}", static_cast<int>(event->type));
            assert(false);
            break;
        }
    }
}

void WorldServer::handleServiceEvent(net::CloseServiceEvent& event)
{
    assert(m_running.load());

    stop();
}

void WorldServer::handleServiceEvent(net::AcceptServiceEvent& event)
{
    if (!m_running.load())
    {
        spdlog::debug("[WorldServer] 서버가 실행 중이 아닙니다. 클라이언트 수락 이벤트를 건너뜁니다.");
        return;
    }

    auto session = net::Session::createInstance(std::move(event.socket), m_sessionEventQueue);
    m_sessionManager.addSession(session);
    session->start();
}

void WorldServer::processSessionEvents()
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
            spdlog::error("[WorldServer] 알 수 없는 세션 이벤트 타입: {}", static_cast<int>(event->type));
            assert(false);
            break;
        }
    }
}

void WorldServer::handleSessionEvent(net::CloseSessionEvent& event)
{
    m_sessionManager.removeSession(event.sessionId);
}

void WorldServer::handleSessionEvent(net::ReceiveSessionEvent& event)
{
    if (!m_running.load())
    {
        spdlog::debug("[WorldServer] 서비스가 실행 중이 아닙니다. 수신 이벤트를 건너뜁니다.");
        return;
    }

    auto session = m_sessionManager.findSession(event.sessionId);
    if (!session)
    {
        spdlog::error("[WorldServer] 세션을 찾을 수 없습니다: {}", event.sessionId);
        assert(false);
        return;
    }

    net::PacketView packet;
    while (session->getFrontPacket(packet))
    {
        if (packet.isValid() == false)
        {
            spdlog::error("[WorldServer] 세션 {}에서 수신된 패킷이 유효하지 않습니다.", event.sessionId);
            session->popPacket();
            continue;
        }

        // TODO: 패킷 처리 로직 추가

        // 패킷 처리 후 수신 버퍼에서 제거
        session->popPacket();
    }

    // 세션에서 다시 비동기 수신 시작
    session->receive();
}
