#pragma once

#include <thread>
#include "Network/Queue.h"
#include "Network/Session.h"
#include "Network/Service.h"

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
    void handleServiceEvent(net::CloseServiceEvent& event);
    void handleServiceEvent(net::AcceptServiceEvent& event);

    void processSessionEvents();
    void handleSessionEvent(net::CloseSessionEvent& event);
    void handleSessionEvent(net::ReceiveSessionEvent& event);

    void broadcastMessage(const std::string& message);

private:
    std::atomic<bool> m_running;
    std::thread m_mainThread;
    net::IoThreadPool m_ioThreadPool;
    net::ServerServicePtr m_serverService;
    net::SessionEventQueue m_sessionEventQueue;
    net::SessionManager m_sessionManager;
    net::SendBufferManager m_sendBufferManager;
};
