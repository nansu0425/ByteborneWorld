#pragma once

#include <thread>
#include "Core/Timer.h"
#include "Network/Session.h"
#include "Network/Service.h"
#include "Network/Event.h"
#include "Protocol/Dispatcher.h"
#include "Protocol/Serializer.h"
#include <unordered_map>
#include <unordered_set>
#include <atomic>

class WorldServer
{
public:
    WorldServer();

    void start();
    void stop();
    void join();

private:
    void loop();
    void close();

    void processServiceEvents();
    void handleServiceEvent(net::ServiceCloseEvent& event);
    void handleServiceEvent(net::ServiceAcceptEvent& event);

    void processSessionEvents();
    void handleSessionEvent(net::SessionCloseEvent& event);
    void handleSessionEvent(net::SessionReceiveEvent& event);

    void processMessages();
    void registerMessageHandlers();
    void handleMessage(net::SessionId sessionId, const proto::C2S_Chat& chat);

private:
    std::atomic<bool> m_running;
    std::thread m_mainThread;
    core::Timer m_timer;
    net::IoThreadPool m_ioThreadPool;
    net::ServiceEventQueue m_serviceEventQueue;
    net::ServerServicePtr m_serverService;
    net::SessionEventQueue m_sessionEventQueue;
    net::SessionManager m_sessionManager;
    net::SendBufferManager m_sendBufferManager;
    proto::MessageQueue m_messageQueue;
    proto::MessageDispatcher m_messageDispatcher;
    proto::MessageSerializer m_messageSerializer;

    // 채팅 권위 정보 관리
    std::atomic<uint64_t> m_nextMessageId{1};
    std::unordered_set<net::SessionId> m_activeSessions;          // 현재 연결 중인 세션 목록
    std::unordered_map<net::SessionId, std::string> m_sessionNames; // 세션별 표시 이름(서버 권위)
};
