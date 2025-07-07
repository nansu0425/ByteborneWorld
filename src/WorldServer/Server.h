#pragma once

#include <asio.hpp>
#include <thread>
#include "Queue.h"
#include "Session.h"

namespace net
{
    class ServerService;
}

class WorldServer
{
public:
    WorldServer();

    void run();
    void stop();
    void join();

private:
    void loop();

    void processIoEvents();
    void handleConnectEvent(const net::SeesionPtr& session);
    void handleDisconnectEvent(const net::SeesionPtr& session);
    void handleRecevieEvent(const net::SeesionPtr& session);

private:
    std::shared_ptr<net::ServerService> m_service;
    net::IoEventQueue m_ioEventQueue;
    net::SessionManager m_sessionManager;
    std::thread m_loopThread;
    bool m_running;
    
};
