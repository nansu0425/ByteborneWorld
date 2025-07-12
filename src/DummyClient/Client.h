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
    void join();

private:
    void loop();
    void close();

    void processServiceEvents();
    void handleServiceEvent(net::CloseServiceEvent& event);
    void handleServiceEvent(net::ConnectServiceEvent& event);

    void processSessionEvents();
    void handleSessionEvent(net::CloseSessionEvent& event);
    void handleSessionEvent(net::ReceiveSessionEvent& event);

private:
    std::atomic<bool> m_running;
    std::thread m_mainThread;
    net::IoThreadPool m_ioThreadPool;
    net::ClientServicePtr m_clientService;
    net::SessionEventQueue m_sessionEventQueue;
    net::SessionManager m_sessionManager;
};
