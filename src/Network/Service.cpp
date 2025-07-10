#include "Pch.h"
#include "Service.h"
#include "Session.h"
#include "Queue.h"

namespace net
{
    IoService::IoService(IoEventQueue& ioEventQueue)
        : m_running(false)
        , m_stopSignals(m_ioThreadPool.getContext())
        , m_ioEventQueue(ioEventQueue)
    {}

    void IoService::asyncWaitForStopSignals(SignalHandler handler)
    {
#if _WIN32
        m_stopSignals.add(SIGINT);
        m_stopSignals.add(SIGTERM);
        m_stopSignals.add(SIGBREAK);
#endif // _WIN32

        m_stopSignals.async_wait(handler);
    }

    ServerIoService::ServerIoService(IoEventQueue& ioEventQueue, uint16_t port)
        : IoService(ioEventQueue)
        , m_acceptor(m_ioThreadPool.getContext(), asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port))
    {}

    ServerIoServicePtr ServerIoService::createInstance(IoEventQueue& ioEventQueue, uint16_t port)
    {
        return std::make_shared<ServerIoService>(ioEventQueue, port);
    }

    void ServerIoService::start(size_t ioThreadCount)
    {
        m_running.store(true);
        SPDLOG_INFO("[ServerIoService] 서비스 시작: {}", m_acceptor.local_endpoint().port());
        
        m_ioThreadPool.run(ioThreadCount);
        asyncAccept();
    }

    void ServerIoService::stop()
    {
        if (m_running.exchange(false))
        {
            SPDLOG_INFO("[ServerIoService] 서비스 중지");

            m_acceptor.cancel();
            m_acceptor.close();
            m_ioThreadPool.stop();
        }
    }

    void ServerIoService::waitForStop()
    {
        m_ioThreadPool.join();

        SPDLOG_INFO("[ServerIoService] 서비스 종료");
    }

    void ServerIoService::asyncAccept()
    {
        if (!m_running.load())
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
            event->session = Session::createInstance(std::move(socket), m_ioEventQueue);
            m_ioEventQueue.push(std::move(event));
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

    ClientIoService::ClientIoService(IoEventQueue& ioEventQueue, const ResolveTarget& resolveTarget)
        : IoService(ioEventQueue)
        , m_socket(m_ioThreadPool.getContext())
        , m_resolver(m_ioThreadPool.getContext())
        , m_resolveTarget(resolveTarget)
    {}

    ClientIoServicePtr ClientIoService::createInstance(IoEventQueue& ioEventQueue, const ResolveTarget& resolveTarget)
    {
        return std::make_shared<ClientIoService>(ioEventQueue, resolveTarget);
    }

    void ClientIoService::start(size_t ioThreadCount)
    {
        m_running.store(true);
        SPDLOG_INFO("[ClientIoService] 서비스 시작");
        
        m_ioThreadPool.run(ioThreadCount);
        asyncResolve();
    }

    void ClientIoService::stop()
    {
        if (m_running.exchange(false))
        {
            SPDLOG_INFO("[ClientIoService] 서비스 중지");

            m_resolver.cancel();
            m_socket.close();
            m_ioThreadPool.stop();
        }
    }

    void ClientIoService::waitForStop()
    {
        m_ioThreadPool.join();

        SPDLOG_INFO("[ClientIoService] 서비스 종료");
    }

    void ClientIoService::asyncResolve()
    {
        if (!m_running.load())
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
        if (!m_running.load())
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
            event->session = Session::createInstance(std::move(m_socket), m_ioEventQueue);
            m_ioEventQueue.push(std::move(event));
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
