#include "Pch.h"
#include "Service.h"
#include "Session.h"
#include "Queue.h"

namespace net
{
    IoService::IoService(size_t ioThreadCount, IoEventQueue& ioEventQueue)
        : m_ioContext()
        , m_ioWorkGuard(asio::make_work_guard(m_ioContext))
        , m_ioThreadCount(ioThreadCount)
        , m_ioEventQueue(ioEventQueue)
    {}

    void IoService::startIoThreads()
    {
        auto self = shared_from_this();
        for (size_t i = 0; i < m_ioThreadCount; ++i)
        {
            m_ioThreads.emplace_back([self]()
                                     {
                                         try
                                         {
                                             self->m_ioContext.run();
                                         }
                                         catch (const std::exception& e)
                                         {
                                             SPDLOG_ERROR("IO 스레드 오류: {}", e.what());
                                         }
                                     });
        }
    }

    void IoService::stopIoThreads()
    {
        m_ioWorkGuard.reset();
        m_ioContext.stop();
    }

    void IoService::joinIoThreads()
    {
        for (auto& thread : m_ioThreads)
        {
            if (thread.joinable())
            {
                thread.join();
            }
        }
        m_ioThreads.clear();
    }

    ServerService::ServerService(uint16_t port, size_t ioThreadCount, IoEventQueue& ioEventQueue)
        : IoService(ioThreadCount, ioEventQueue)
        , m_acceptor(getIoContext(), asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port))
    {}

    void ServerService::start()
    {
        startIoThreads();
        asyncAccept();
        SPDLOG_INFO("서버 서비스가 시작되었습니다. 포트: {}", m_acceptor.local_endpoint().port());
    }

    void ServerService::stop()
    {
        m_acceptor.close();
        stopIoThreads();
        SPDLOG_INFO("서버 서비스가 중지되었습니다.");
    }

    void ServerService::join()
    {
        joinIoThreads();
        SPDLOG_INFO("서버 서비스가 종료되었습니다.");
    }

    void ServerService::asyncAccept()
    {
        auto self = getInstance();
        m_acceptor.async_accept([self](const asio::error_code& error, asio::ip::tcp::socket socket)
                                {
                                    self->onAccepted(error, std::move(socket));
                                });
    }

    void ServerService::onAccepted(const asio::error_code& error, asio::ip::tcp::socket socket)
    {
        if (!error)
        {
            SPDLOG_INFO("새로운 연결이 수락되었습니다. 소켓: {}", socket.remote_endpoint().address().to_string());
            // 이벤트 큐에 연결 이벤트 추가
            IoEventPtr event = std::make_shared<IoEvent>();
            event->type = IoEventType::Connect;
            event->session = SessionManager::createSession(std::move(socket), getIoEventQueue());
            getIoEventQueue().push(event);
        }
        else
        {
            SPDLOG_ERROR("연결 수락 오류: {}", error.value());
        }
        // 다음 연결을 수락하기 위해 다시 호출
        asyncAccept();
    }

    ClientService::ClientService(const std::string& host, uint16_t port, size_t ioThreadCount, IoEventQueue& ioEventQueue)
        : IoService(ioThreadCount, ioEventQueue)
        , m_socket(getIoContext())
        , m_resolver(getIoContext())
        , m_host(host)
        , m_port(port)
    {}

    void ClientService::start()
    {
        startIoThreads();
        asyncResolve();
        SPDLOG_INFO("클라이언트 서비스가 시작되었습니다. 호스트: {}, 포트: {}", m_host, m_port);
    }

    void ClientService::stop()
    {
        m_socket.close();
        stopIoThreads();
        SPDLOG_INFO("클라이언트 서비스가 중지되었습니다.");
    }

    void ClientService::join()
    {
        joinIoThreads();
        SPDLOG_INFO("클라이언트 서비스가 종료되었습니다.");
    }

    void ClientService::asyncResolve()
    {
        auto self = getInstance();
        m_resolver.async_resolve(m_host, std::to_string(m_port),
                                 [self](const asio::error_code& error, asio::ip::tcp::resolver::results_type results)
                                 {
                                     self->onResolved(error, results);
                                 });
    }

    void ClientService::onResolved(const asio::error_code& error, asio::ip::tcp::resolver::results_type results)
    {
        if (!error)
        {
            SPDLOG_INFO("호스트 {}:{}를 성공적으로 해석했습니다.", m_host, m_port);
            m_resolvedEndpoints = results;
            asyncConnect();
        }
        else
        {
            SPDLOG_ERROR("호스트 해석 오류: {}", error.value());
        }
    }

    void ClientService::asyncConnect()
    {
        auto self = getInstance();
        asio::async_connect(m_socket, m_resolvedEndpoints,
                            [self](const asio::error_code& error, const asio::ip::tcp::endpoint&)
                            {
                                self->onConnected(error);
                            });
    }

    void ClientService::onConnected(const asio::error_code& error)
    {
        if (!error)
        {
            SPDLOG_INFO("서버 {}:{}에 성공적으로 연결되었습니다.", m_host, m_port);
            // 이벤트 큐에 연결 이벤트 추가
            IoEventPtr event = std::make_shared<IoEvent>();
            event->type = IoEventType::Connect;
            event->session = SessionManager::createSession(std::move(m_socket), getIoEventQueue());
            getIoEventQueue().push(event);
        }
        else
        {
            SPDLOG_ERROR("연결 오류: {}", error.value());
        }
    }
}
