﻿#pragma once

#include <thread>
#include "Core/Timer.h"
#include "Network/Session.h"
#include "Network/Service.h"
#include "Network/Event.h"
#include "Protocol/Dispatcher.h"
#include "Protocol/Serializer.h"

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
};
