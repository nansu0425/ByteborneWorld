﻿#include "Client.h"
#include "Network/Packet.h"
#include "Protocol/Type.h"
#include <chrono>

DummyClient::DummyClient()
    : m_running(false)
    , m_messageSerializer(m_sendBufferManager)
{
    m_clientService = net::ClientService::createInstance(
        m_ioThreadPool.getContext(), m_serviceEventQueue, net::ResolveTarget{"localhost", "12345"}, 10);

    registerMessageHandlers();
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
        processMessages();
        m_timer.update();
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

    // 모든 세션이 제거될 때까지 대기
    while (m_sessionManager.isEmpty() == false)
    {
        processSessionEvents();
        std::this_thread::yield();
    }

    // io 스레드가 처리할 핸들러가 없으면 스레드가 종료되도록 설정
    m_ioThreadPool.reset();
}

void DummyClient::processServiceEvents()
{
    net::ServiceEventPtr event;
    while (m_serviceEventQueue.pop(event))
    {
        switch (event->type)
        {
        case net::ServiceEventType::Close:
            handleServiceEvent(*static_cast<net::ServiceCloseEvent*>(event.get()));
            break;
        case net::ServiceEventType::Connect:
            handleServiceEvent(*static_cast<net::ServiceConnectEvent*>(event.get()));
            break;
        default:
            spdlog::error("[DummyClient] 알 수 없는 서비스 이벤트 타입: {}", static_cast<int>(event->type));
            assert(false);
            break;
        }
    }
}

void DummyClient::handleServiceEvent(net::ServiceCloseEvent& event)
{
    assert(m_running.load());

    stop();
}

void DummyClient::handleServiceEvent(net::ServiceConnectEvent& event)
{
    if (!m_running.load())
    {
        spdlog::debug("[DummyClient] 클라이언트가 실행 중이 아닙니다. 서버 연결 이벤트를 건너뜁니다.");
        return;
    }

    auto session = net::Session::createInstance(std::move(event.socket), m_sessionEventQueue);
    m_sessionManager.addSession(session);
    session->start();

    // 테스트: 주기적으로 C2S_Chat 전송(권위 필드 포함)
    m_timer.scheduleRepeating(
        std::chrono::milliseconds(0),
        std::chrono::milliseconds(500),
        [this, sessionId = session->getSessionId()]()
        {
            if (!m_running.load())
                return false;

            const uint64_t clientMessageId = m_nextClientMessageId.fetch_add(1);
            const int64_t clientSentAtMs = [](){
                using namespace std::chrono; return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
            }();

            proto::C2S_Chat chat;
            chat.set_sender_name("더미"); // 서버는 신뢰하지 않음
            chat.set_content("Hello, Byteborne World!");
            chat.set_client_message_id(clientMessageId);
            chat.set_client_sent_at_ms(clientSentAtMs);

            net::SendBufferChunkPtr chunk =  m_messageSerializer.serializeToSendBuffer(chat);
            if (m_sessionManager.send(sessionId, chunk) == false)
            {
                spdlog::warn("[DummyClient] 세션 {} 전송 실패", sessionId);
                return false;
            }

            return true; // continue scheduling
        }
    );
}

void DummyClient::processSessionEvents()
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
            spdlog::error("[DummyClient] 알 수 없는 세션 이벤트 타입: {}", static_cast<int>(event->type));
            assert(false);
            break;
        }
    }
}

void DummyClient::handleSessionEvent(net::SessionCloseEvent& event)
{
    m_sessionManager.removeSession(event.sessionId);
}

void DummyClient::handleSessionEvent(net::SessionReceiveEvent& event)
{
    if (!m_running.load())
    {
        spdlog::debug("[DummyClient] 클라이언트가 실행 중이 아닙니다. 수신 이벤트를 건너뜁니다.");
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

void DummyClient::processMessages()
{
    while (m_running.load() && (m_messageQueue.isEmpty() == false))
    {
        m_messageDispatcher.dispatch(m_messageQueue.front());
        m_messageQueue.pop();
    }
}

void DummyClient::registerMessageHandlers()
{
    m_messageDispatcher.registerHandler(
        proto::MessageType::S2C_Chat,
        [this](net::SessionId sessionId, const proto::MessagePtr& message)
        {
            handleMessage(sessionId, *std::static_pointer_cast<proto::S2C_Chat>(message));
        });
}

void DummyClient::handleMessage(net::SessionId sessionId, const proto::S2C_Chat& message)
{
    spdlog::info(
        "[DummyClient] Session {}: S2C_Chat 수신: sender='{}' content='{}' smid={} cmid={} at={}",
        sessionId,
        message.sender_name(),
        message.content(),
        message.server_message_id(),
        message.client_message_id(),
        message.server_sent_at_ms());
}
