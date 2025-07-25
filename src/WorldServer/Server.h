#pragma once

#include <thread>
#include "Network/Session.h"
#include "Network/Service.h"
#include "Network/Event.h"
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

    void broadcastMessage(const std::string& message);

private:
    std::atomic<bool> m_running;
    std::thread m_mainThread;
    net::IoThreadPool m_ioThreadPool;
    net::ServiceEventQueue m_serviceEventQueue;
    net::ServerServicePtr m_serverService;
    net::SessionEventQueue m_sessionEventQueue;
    net::SessionManager m_sessionManager;
    net::SendBufferManager m_sendBufferManager;
    proto::MessageSerializer m_messageSerializer;
};
