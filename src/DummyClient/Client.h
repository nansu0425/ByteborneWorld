#pragma once

#include <thread>
#include "Network/Queue.h"
#include "Network/Session.h"
#include "Network/Service.h"

class DummyClient
{
public:
    DummyClient();

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
    net::ClientIoServicePtr m_clientIoService;
    net::IoEventQueue m_ioEventQueue;
    net::SessionManager m_sessionManager;
    std::thread m_mainLoopThread;
    std::atomic<bool> m_running;
};
