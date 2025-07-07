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
        for (size_t i = 0; i < m_ioThreadCount; ++i)
        {
            m_ioThreads.emplace_back([self = shared_from_this()]()
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

    ServerIoServicePtr ServerIoService::createInstance(uint16_t port, size_t ioThreadCount, IoEventQueue& ioEventQueue)
    {
        return std::shared_ptr<ServerIoService>(new ServerIoService(port, ioThreadCount, ioEventQueue));
    }

    void ServerIoService::start()
    {
        startIoThreads();
        asyncAccept();
        SPDLOG_INFO("서버 서비스가 시작되었습니다. 포트: {}", m_acceptor.local_endpoint().port());
    }

    void ServerIoService::stop()
    {
        m_acceptor.close();
        stopIoThreads();
        SPDLOG_INFO("서버 서비스가 중지되었습니다.");
    }

    void ServerIoService::join()
    {
        joinIoThreads();
        SPDLOG_INFO("서버 서비스가 종료되었습니다.");
    }

    ServerIoService::ServerIoService(uint16_t port, size_t ioThreadCount, IoEventQueue& ioEventQueue)
        : IoService(ioThreadCount, ioEventQueue)
        , m_acceptor(getIoContext(), asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port))
    {}

    void ServerIoService::asyncAccept()
    {
        m_acceptor.async_accept([self = getInstance()](const asio::error_code& error, asio::ip::tcp::socket socket)
                                {
                                    self->onAccepted(error, std::move(socket));
                                });
    }

    void ServerIoService::onAccepted(const asio::error_code& error, asio::ip::tcp::socket socket)
    {
        if (!error)
        {
            SPDLOG_INFO("새로운 연결이 수락되었습니다. 소켓: {}", socket.remote_endpoint().address().to_string());
            // 이벤트 큐에 연결 이벤트 추가
            IoEventPtr event = std::make_shared<IoEvent>();
            event->type = IoEventType::Connect;
            event->session = Session::createInstance(std::move(socket), getIoEventQueue());
            getIoEventQueue().push(event);
        }
        else
        {
            SPDLOG_ERROR("연결 수락 오류: {}", error.value());
        }

        // 다음 연결을 수락하기 위해 다시 호출
        asyncAccept();
    }

    ClientIoServicePtr ClientIoService::createInstance(const std::string& host, uint16_t port, size_t ioThreadCount, IoEventQueue& ioEventQueue)
    {
        return std::shared_ptr<ClientIoService>(new ClientIoService(host, port, ioThreadCount, ioEventQueue));
    }

    void ClientIoService::start()
    {
        startIoThreads();
        asyncResolve();
        SPDLOG_INFO("클라이언트 서비스가 시작되었습니다. 호스트: {}, 포트: {}", m_host, m_port);
    }

    void ClientIoService::stop()
    {
        m_socket.close();
        stopIoThreads();
        SPDLOG_INFO("클라이언트 서비스가 중지되었습니다.");
    }

    void ClientIoService::join()
    {
        joinIoThreads();
        SPDLOG_INFO("클라이언트 서비스가 종료되었습니다.");
    }

    ClientIoService::ClientIoService(const std::string& host, uint16_t port, size_t ioThreadCount, IoEventQueue& ioEventQueue)
        : IoService(ioThreadCount, ioEventQueue)
        , m_socket(getIoContext())
        , m_resolver(getIoContext())
        , m_host(host)
        , m_port(port)
    {}

    void ClientIoService::asyncResolve()
    {
        m_resolver.async_resolve(m_host, std::to_string(m_port),
                                 [self = getInstance()](const asio::error_code& error, asio::ip::tcp::resolver::results_type results)
                                 {
                                     self->onResolved(error, results);
                                 });
    }

    void ClientIoService::onResolved(const asio::error_code& error, asio::ip::tcp::resolver::results_type results)
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

    void ClientIoService::asyncConnect()
    {
        asio::async_connect(m_socket, m_resolvedEndpoints,
                            [self = getInstance()](const asio::error_code& error, const asio::ip::tcp::endpoint&)
                            {
                                self->onConnected(error);
                            });
    }

    void ClientIoService::onConnected(const asio::error_code& error)
    {
        if (!error)
        {
            SPDLOG_INFO("서버 {}:{}에 성공적으로 연결되었습니다.", m_host, m_port);
            // 이벤트 큐에 연결 이벤트 추가
            IoEventPtr event = std::make_shared<IoEvent>();
            event->type = IoEventType::Connect;
            event->session = Session::createInstance(std::move(m_socket), getIoEventQueue());
            getIoEventQueue().push(event);
        }
        else
        {
            SPDLOG_ERROR("연결 오류: {}", error.value());
        }
    }
}
