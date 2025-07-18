#pragma once

#include <thread>
#include "Network/Session.h"
#include "Network/Service.h"
#include "Network/Event.h"

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
    void handleServiceEvent(net::ServiceCloseEvent& event);
    void handleServiceEvent(net::ServiceConnectEvent& event);

    void processSessionEvents();
    void handleSessionEvent(net::SessionCloseEvent& event);
    void handleSessionEvent(net::SessionReceiveEvent& event);

private:
    std::atomic<bool> m_running;
    std::thread m_mainThread;
    net::IoThreadPool m_ioThreadPool;
    net::ServiceEventQueue m_serviceEventQueue;
    net::ClientServicePtr m_clientService;
    net::SessionEventQueue m_sessionEventQueue;
    net::SessionManager m_sessionManager;
};
