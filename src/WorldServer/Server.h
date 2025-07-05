#pragma once

#include <asio.hpp>
#include <thread>
#include "Service.h"
#include "Queue.h"
#include "Session.h"

class WorldServer
{
public:
    WorldServer();

    void run();
    void stop();
    void join();

private:
    void loop();
    void ProcessSessionEvents();
    void onRecevied(const net::SeesionPtr& session);

private:
    asio::io_context m_ioContext;
    asio::executor_work_guard<asio::io_context::executor_type> m_ioWorkGuard;
    std::vector<std::thread> m_ioThreads;
    net::SessionEventQueue m_sessionEventQueue;
    std::shared_ptr<net::AcceptService> m_acceptService;
    net::SessionManager m_sessionManager;
    std::thread m_loopThread;
    bool m_running;
    
};
