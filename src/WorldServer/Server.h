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
    void join();

private:
    void loop();

    void processIoEvents();
    void handleConnectEvent(const net::SessionPtr& session);
    void handleDisconnectEvent(const net::SessionPtr& session);
    void handleRecevieEvent(const net::SessionPtr& session);

private:
    net::ServerIoServicePtr m_serverIoService;
    net::IoEventQueue m_ioEventQueue;
    net::SessionManager m_sessionManager;
    std::thread m_loopThread;
    bool m_running;
    
};
