#pragma once

#include <asio.hpp>
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
    void watiForStop();

private:
    void runMainLoop();

    void processIoEvents();
    void handleConnectEvent(const net::SessionPtr& session);
    void handleDisconnectEvent(const net::SessionPtr& session);
    void handleRecevieEvent(const net::SessionPtr& session);

private:
    net::ServerIoServicePtr m_serverIoService;
    net::IoEventQueue m_ioEventQueue;
    net::SessionManager m_sessionManager;
    std::thread m_mainLoopThread;
    std::atomic<bool> m_running;
    
};
