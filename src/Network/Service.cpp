#include "Pch.h"
#include "Service.h"
#include "Session.h"
#include "Queue.h"

namespace net
{
    IoService::IoService(size_t ioThreadCount, IoEventQueue& ioEventQueue)
        : m_state(IoServiceState::Stopped)
        , m_ioContext()
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
                                             SPDLOG_ERROR("[IoService] IO 스레드 오류: {}", e.what());
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

    ServerIoService::ServerIoService(uint16_t port, size_t ioThreadCount, IoEventQueue& ioEventQueue)
        : IoService(ioThreadCount, ioEventQueue)
        , m_acceptor(getIoContext(), asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port))
    {}

    ServerIoServicePtr ServerIoService::createInstance(uint16_t port, size_t ioThreadCount, IoEventQueue& ioEventQueue)
    {
        return std::make_shared<ServerIoService>(port, ioThreadCount, ioEventQueue);
    }

    void ServerIoService::start()
    {
        m_state.store(IoServiceState::Running);
        auto port = m_acceptor.local_endpoint().port();
        SPDLOG_INFO("[ServerIoService] 서비스 시작: {}", port);
        
        startIoThreads();
        asyncAccept();
    }

    void ServerIoService::stop()
    {
        assert(m_state.load() != IoServiceState::Stopped);
        IoServiceState state = m_state.exchange(IoServiceState::Stopping);

        if (state == IoServiceState::Running)
        {
            SPDLOG_INFO("[ServerIoService] 서비스 중지");

            m_acceptor.cancel();
            m_acceptor.close();
            stopIoThreads();
        }
    }

    void ServerIoService::join()
    {
        joinIoThreads();

        m_state.store(IoServiceState::Stopped);
        SPDLOG_INFO("[ServerIoService] 서비스 종료");
    }

    void ServerIoService::asyncAccept()
    {
        if (m_state.load() != IoServiceState::Running)
        {
            return;
        }

        m_acceptor.async_accept([self = getInstance()](const asio::error_code& error, asio::ip::tcp::socket socket)
                                {
                                    self->onAccepted(error, std::move(socket));
                                });
    }

    void ServerIoService::onAccepted(const asio::error_code& error, asio::ip::tcp::socket socket)
    {
        if (!error)
        {
            const auto& remoteEndpoint = socket.remote_endpoint();
            SPDLOG_INFO("[ServerIoService] 연결 수락: {}:{}", remoteEndpoint.address().to_string(), remoteEndpoint.port());

            // 이벤트 큐에 연결 이벤트 추가
            IoEventPtr event = std::make_shared<IoEvent>();
            event->type = IoEventType::Connect;
            event->session = Session::createInstance(std::move(socket), getIoEventQueue());
            getIoEventQueue().push(std::move(event));
        }
        else
        {
            handleError(error);
        }

        // 다음 연결을 수락하기 위해 다시 호출
        asyncAccept();
    }

    void ServerIoService::handleError(const asio::error_code& error)
    {
        switch (error.value())
        {
        case asio::error::operation_aborted:
            SPDLOG_WARN("[ServerIoService] operation_aborted");
            break;
        case asio::error::invalid_argument:
            SPDLOG_ERROR("[ServerIoService] invalid_argument");
            break;
        case asio::error::connection_aborted:
            SPDLOG_WARN("[ServerIoService] connection_aborted");
            break;
        case asio::error::connection_reset:
            SPDLOG_WARN("[ServerIoService] connection_reset");
            break;
        case asio::error::timed_out:
            SPDLOG_WARN("[ServerIoService] timed_out");
            break;
        case asio::error::connection_refused:
            SPDLOG_WARN("[ServerIoService] connection_refused");
            break;
        case asio::error::bad_descriptor:
            SPDLOG_ERROR("[ServerIoService] bad_descriptor");
            assert(false);
            stop();
            break;
        default:
            SPDLOG_ERROR("[ServerIoService] 알 수 없는 에러: {}", error.value());
            assert(false);
            stop();
            break;
        }
    }

    ClientIoService::ClientIoService(const ResolveTarget& resolveTarget, size_t ioThreadCount, IoEventQueue& ioEventQueue)
        : IoService(ioThreadCount, ioEventQueue)
        , m_socket(getIoContext())
        , m_resolver(getIoContext())
        , m_resolveTarget(resolveTarget)
    {}

    ClientIoServicePtr ClientIoService::createInstance(const ResolveTarget& resolveTarget, size_t ioThreadCount, IoEventQueue& ioEventQueue)
    {
        return std::make_shared<ClientIoService>(resolveTarget, ioThreadCount, ioEventQueue);
    }

    void ClientIoService::start()
    {
        m_state.store(IoServiceState::Running);
        SPDLOG_INFO("[ClientIoService] 서비스 시작");
        
        startIoThreads();
        asyncResolve();
    }

    void ClientIoService::stop()
    {
        assert(m_state.load() != IoServiceState::Stopped);
        IoServiceState state = m_state.exchange(IoServiceState::Stopping);

        if (state == IoServiceState::Running)
        {
            SPDLOG_INFO("[ClientIoService] 서비스 중지");

            m_resolver.cancel();
            m_socket.cancel();
            m_socket.close();
            stopIoThreads();
        }
    }

    void ClientIoService::join()
    {
        joinIoThreads();

        m_state.store(IoServiceState::Stopped);
        SPDLOG_INFO("[ClientIoService] 서비스 종료");
    }

    void ClientIoService::asyncResolve()
    {
        if (m_state.load() != IoServiceState::Running)
        {
            return;
        }

        m_resolver.async_resolve(m_resolveTarget.host, m_resolveTarget.service,
                                 [self = getInstance()]
                                 (const asio::error_code& error, const asio::ip::tcp::resolver::results_type& results)
                                 {
                                     self->onResolved(error, results);
                                 });
    }

    void ClientIoService::onResolved(const asio::error_code& error, const asio::ip::tcp::resolver::results_type& results)
    {
        if (!error)
        {
            for (const auto& entry : results)
            {
                const auto& endpoint = entry.endpoint();
                SPDLOG_INFO("[ClientIoService] endpoint: {}:{}", endpoint.address().to_string(), endpoint.port());
            }

            m_resolveResults = results;
            asyncConnect();
        }
        else
        {
            handleError(error);
        }
    }

    void ClientIoService::asyncConnect()
    {
        if (m_state.load() != IoServiceState::Running)
        {
            return;
        }

        asio::async_connect(m_socket, m_resolveResults,
                            [self = getInstance()](const asio::error_code& error, const asio::ip::tcp::endpoint&)
                            {
                                self->onConnected(error);
                            });
    }

    void ClientIoService::onConnected(const asio::error_code& error)
    {
        if (!error)
        {
            SPDLOG_INFO("[ClientIoService] 연결 성공: {}:{}", m_resolveTarget.host, m_resolveTarget.service);

            // 이벤트 큐에 연결 이벤트 추가
            IoEventPtr event = std::make_shared<IoEvent>();
            event->type = IoEventType::Connect;
            event->session = Session::createInstance(std::move(m_socket), getIoEventQueue());
            getIoEventQueue().push(std::move(event));
        }
        else
        {
            handleError(error);
        }
    }

    void ClientIoService::handleError(const asio::error_code& error)
    {
        switch (error.value())
        {
        case asio::error::operation_aborted:
            SPDLOG_WARN("[ClientIoService] operation_aborted");
            break;
        case asio::error::host_not_found:
            SPDLOG_ERROR("[ClientIoService] host_not_found");
            stop();
            break;
        case asio::error::service_not_found:
            SPDLOG_ERROR("[ClientIoService] service_not_found");
            stop();
            break;
        case asio::error::connection_refused:
            SPDLOG_ERROR("[ClientIoService] connection_refused");
            stop();
            break;
        case asio::error::timed_out:
            SPDLOG_ERROR("[ClientIoService] timed_out");
            stop();
            break;
        case asio::error::network_unreachable:
            SPDLOG_ERROR("[ClientIoService] network_unreachable");
            stop();
            break;
        case asio::error::connection_aborted:
            SPDLOG_ERROR("[ClientIoService] connection_aborted");
            stop();
            break;
        case asio::error::already_connected:
            SPDLOG_ERROR("[ClientIoService] already_connected");
            assert(false);
            stop();
            break;
        case asio::error::bad_descriptor:
            SPDLOG_ERROR("[ClientIoService] bad_descriptor");
            assert(false);
            stop();
            break;
        default:
            SPDLOG_ERROR("[ClientIoService] 알 수 없는 에러: {}", error.value());
            assert(false);
            stop();
            break;
        }
    }
}
