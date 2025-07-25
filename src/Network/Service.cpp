#include "Service.h"
#include "Session.h"
#include "Event.h"

namespace net
{
    Service::Service(asio::io_context& ioContext, ServiceEventQueue& eventQueue)
        : m_running(false)
        , m_strand(asio::make_strand(ioContext))
        , m_stopSignals(ioContext)
        , m_eventQueue(eventQueue)
    {}

    void Service::asyncWaitForStopSignals()
    {
#if _WIN32
        m_stopSignals.add(SIGINT);
        m_stopSignals.add(SIGTERM);
        m_stopSignals.add(SIGBREAK);
#endif // _WIN32

        m_stopSignals.async_wait(
            [this, self = shared_from_this()]
            (const asio::error_code& error, int signalNumber)
            {
                if (!error)
                {
                    spdlog::debug("[Service] 중지 신호 수신: {}", signalNumber);
                }
                else
                {
                    handleError(error);
                }

                stop();
            });
    }

    ServerService::ServerService(asio::io_context& ioContext, ServiceEventQueue& eventQueue, uint16_t port)
        : Service(ioContext, eventQueue)
        , m_acceptor(ioContext, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port))
    {}

    ServerServicePtr ServerService::createInstance(asio::io_context& ioContext, ServiceEventQueue& eventQueue, uint16_t port)
    {
        auto service = std::make_shared<ServerService>(ioContext, eventQueue, port);
        service->asyncWaitForStopSignals();

        return service;
    }

    void ServerService::start()
    {
        if (m_running.exchange(true))
        {
            // 이미 실행 중인 경우 아무 작업도 하지 않음
            return;
        }

        spdlog::info("[ServerService] 서비스 시작: {}", m_acceptor.local_endpoint().port());

        asio::post(
            m_strand,
            [this, self = shared_from_this()]()
            {
                asyncAccept();
            });
    }

    void ServerService::stop()
    {
        if (!m_running.exchange(false))
        {
            // 이미 중지 상태이므로 아무 작업도 하지 않음
            return;
        }

        spdlog::info("[ServerService] 서비스 중지");

        asio::post(
            m_strand,
            [this, self = shared_from_this()]()
            {
                close();
            });
    }

    void ServerService::asyncAccept()
    {
        if (!m_running.load())
        {
            return;
        }

        m_acceptor.async_accept(
            asio::bind_executor(
                m_strand,
                [this, self = shared_from_this()]
                (const asio::error_code& error, asio::ip::tcp::socket socket)
                {
                    onAccepted(error, std::move(socket));
                }));
    }

    void ServerService::onAccepted(const asio::error_code& error, asio::ip::tcp::socket&& socket)
    {
        if (!error)
        {
            const auto& remoteEndpoint = socket.remote_endpoint();
            spdlog::debug("[ServerService] 클라이언트 수락: {}:{}", remoteEndpoint.address().to_string(), remoteEndpoint.port());

            // 이벤트 큐에 accept 이벤트 추가
            ServiceEventPtr event = std::make_shared<ServiceAcceptEvent>(std::move(socket));
            m_eventQueue.push(std::move(event));
        }
        else
        {
            handleError(error);
        }

        // 다음 accept를 위해 다시 호출
        asyncAccept();
    }

    void ServerService::handleError(const asio::error_code& error)
    {
        switch (error.value())
        {
        case asio::error::operation_aborted:
            spdlog::debug("[ServerService] operation_aborted");
            break;
        case asio::error::invalid_argument:
            spdlog::error("[ServerService] invalid_argument");
            break;
        case asio::error::connection_aborted:
            spdlog::debug("[ServerService] connection_aborted");
            break;
        case asio::error::connection_reset:
            spdlog::debug("[ServerService] connection_reset");
            break;
        case asio::error::timed_out:
            spdlog::debug("[ServerService] timed_out");
            break;
        case asio::error::connection_refused:
            spdlog::debug("[ServerService] connection_refused");
            break;
        case asio::error::bad_descriptor:
            spdlog::error("[ServerService] bad_descriptor");
            assert(false);
            stop();
            break;
        default:
            spdlog::error("[ServerService] 알 수 없는 에러: {}", error.value());
            assert(false);
            stop();
            break;
        }
    }

    void ServerService::close()
    {
        assert(!m_running.load());

        spdlog::info("[ServerService] 서비스 닫기");

        asio::error_code error;

        m_stopSignals.cancel(error);
        if (error)
        {
            handleError(error);
        }

        if (m_acceptor.is_open())
        {
            m_acceptor.cancel(error);
            if (error)
            {
                handleError(error);
            }

            m_acceptor.close(error);
            if (error)
            {
                handleError(error);
            }
        }

        // 서비스 이벤트 큐에 close 이벤트 추가
        ServiceEventPtr event = std::make_shared<ServiceCloseEvent>();
        m_eventQueue.push(std::move(event));
    }

    ClientService::ClientService(asio::io_context& ioContext, ServiceEventQueue& eventQueue, const ResolveTarget& resolveTarget, size_t connectCount)
        : Service(ioContext, eventQueue)
        , m_resolveTarget(resolveTarget)
        , m_resolver(ioContext)
    {
        assert(connectCount > 0);

        m_sockets.reserve(connectCount);
        for (size_t i = 0; i < connectCount; ++i)
        {
            m_sockets.emplace_back(ioContext);
        }
    }

    ClientServicePtr ClientService::createInstance(asio::io_context& ioContext, ServiceEventQueue& eventQueue, const ResolveTarget& resolveTarget, size_t connectCount)
    {
        auto service = std::make_shared<ClientService>(ioContext, eventQueue, resolveTarget, connectCount);
        service->asyncWaitForStopSignals();

        return service;
    }

    void ClientService::start()
    {
        if (m_running.exchange(true))
        {
            // 이미 실행 중인 경우 아무 작업도 하지 않음
            return;
        }

        spdlog::info("[ClientService] 서비스 시작");

        asio::post(
            m_strand,
            [this, self = shared_from_this()]()
            {
                asyncResolve();
            });
    }

    void ClientService::stop()
    {
        if (!m_running.exchange(false))
        {
            // 이미 중지 상태이므로 아무 작업도 하지 않음
            return;
        }

        spdlog::info("[ClientService] 서비스 중지");

        asio::post(
            m_strand,
            [this, self = shared_from_this()]()
            {
                close();
            });
    }

    void ClientService::asyncResolve()
    {
        if (!m_running.load())
        {
            return;
        }

        m_resolver.async_resolve(
            m_resolveTarget.host, m_resolveTarget.service,
            asio::bind_executor(
                m_strand,
                [this, self = shared_from_this()]
                (const asio::error_code& error, const asio::ip::tcp::resolver::results_type& results)
                {
                    onResolved(error, results);
                }));         
    }

    void ClientService::onResolved(const asio::error_code& error, const asio::ip::tcp::resolver::results_type& results)
    {
        if (!error)
        {
            for (const auto& entry : results)
            {
                const auto& endpoint = entry.endpoint();
                spdlog::debug("[ClientService] 엔드포인트: {}:{}", endpoint.address().to_string(), endpoint.port());
            }

            m_resolveResults = results;

            // 비동기 연결 시작
            for (size_t i = 0; i < m_sockets.size(); ++i)
            {
                asyncConnect(i);
            }
        }
        else
        {
            handleError(error);
        }
    }

    void ClientService::asyncConnect(size_t socketIndex)
    {
        if (!m_running.load())
        {
            return;
        }

        asio::async_connect(
            m_sockets[socketIndex], m_resolveResults,
            asio::bind_executor(
                m_strand,
                [this, self = shared_from_this(), socketIndex]
                (const asio::error_code& error, const asio::ip::tcp::endpoint& endpoint)
                {
                    onConnected(error, socketIndex);
                }));
    }

    void ClientService::onConnected(const asio::error_code& error, size_t socketIndex)
    {
        if (!error)
        {
            spdlog::debug("[ClientService] 서버 연결: {}:{}", m_resolveTarget.host, m_resolveTarget.service);

            // 이벤트 큐에 connect 이벤트 추가
            ServiceEventPtr event = std::make_shared<ServiceConnectEvent>(std::move(m_sockets[socketIndex]));
            m_eventQueue.push(std::move(event));
        }
        else
        {
            handleError(error);
        }
    }

    void ClientService::handleError(const asio::error_code& error)
    {
        switch (error.value())
        {
        case asio::error::operation_aborted:
            spdlog::debug("[ClientService] operation_aborted");
            break;
        case asio::error::host_not_found:
            spdlog::error("[ClientService] host_not_found");
            stop();
            break;
        case asio::error::service_not_found:
            spdlog::error("[ClientService] service_not_found");
            stop();
            break;
        case asio::error::connection_refused:
            spdlog::error("[ClientService] connection_refused");
            stop();
            break;
        case asio::error::timed_out:
            spdlog::error("[ClientService] timed_out");
            stop();
            break;
        case asio::error::network_unreachable:
            spdlog::error("[ClientService] network_unreachable");
            stop();
            break;
        case asio::error::connection_aborted:
            spdlog::error("[ClientService] connection_aborted");
            stop();
            break;
        case asio::error::already_connected:
            spdlog::error("[ClientService] already_connected");
            assert(false);
            stop();
            break;
        case asio::error::bad_descriptor:
            spdlog::error("[ClientService] bad_descriptor");
            assert(false);
            stop();
            break;
        default:
            spdlog::error("[ClientService] 알 수 없는 에러: {}", error.value());
            assert(false);
            stop();
            break;
        }
    }

    void ClientService::close()
    {
        assert(!m_running.load());

        spdlog::info("[ClientService] 서비스 닫기");

        asio::error_code error;

        m_stopSignals.cancel(error);
        if (error)
        {
            handleError(error);
        }

        m_resolver.cancel();

        for (auto& socket : m_sockets)
        {
            if (socket.is_open())
            {
                // 모든 비동기 작업을 취소
                socket.cancel(error);
                if (error)
                {
                    handleError(error);
                }

                // 소켓 닫기
                socket.close(error);
                if (error)
                {
                    handleError(error);
                }
            }
        }

        // 서비스 이벤트 큐에 close 이벤트 추가
        ServiceEventPtr event = std::make_shared<ServiceCloseEvent>();
        m_eventQueue.push(std::move(event));
    }
}
