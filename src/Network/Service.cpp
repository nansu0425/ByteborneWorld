#include "Pch.h"
#include "Service.h"
#include "Session.h"
#include "Queue.h"

namespace net
{
    Service::Service(asio::io_context& ioContext)
        : m_running(false)
        , m_strand(asio::make_strand(ioContext))
        , m_stopSignals(ioContext)
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
                    SPDLOG_INFO("[Service] 중지 신호 수신: {}", signalNumber);
                }
                else
                {
                    SPDLOG_ERROR("[Service] 중지 신호 처리 중 에러 발생: {}", error.value());
                }

                // 서비스 중지
                stop();
            });
    }

    ServerService::ServerService(asio::io_context& ioContext, uint16_t port)
        : Service(ioContext)
        , m_acceptor(ioContext, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port))
    {}

    ServerServicePtr ServerService::createInstance(asio::io_context& ioContext, uint16_t port)
    {
        auto service = std::make_shared<ServerService>(ioContext, port);
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

        SPDLOG_INFO("[ServerService] 서비스 시작: {}", m_acceptor.local_endpoint().port());

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

        SPDLOG_INFO("[ServerService] 서비스 중지");

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
            SPDLOG_INFO("[ServerService] 클라이언트 수락: {}:{}", remoteEndpoint.address().to_string(), remoteEndpoint.port());

            // 이벤트 큐에 클라이언트 수락 이벤트 추가
            ServiceEventPtr event = std::make_shared<AcceptServiceEvent>(std::move(socket));
            m_eventQueue.push(std::move(event));
        }
        else
        {
            handleError(error);
        }

        // 다음 연결을 수락하기 위해 다시 호출
        asyncAccept();
    }

    void ServerService::handleError(const asio::error_code& error)
    {
        switch (error.value())
        {
        case asio::error::operation_aborted:
            SPDLOG_WARN("[ServerService] operation_aborted");
            break;
        case asio::error::invalid_argument:
            SPDLOG_ERROR("[ServerService] invalid_argument");
            break;
        case asio::error::connection_aborted:
            SPDLOG_WARN("[ServerService] connection_aborted");
            break;
        case asio::error::connection_reset:
            SPDLOG_WARN("[ServerService] connection_reset");
            break;
        case asio::error::timed_out:
            SPDLOG_WARN("[ServerService] timed_out");
            break;
        case asio::error::connection_refused:
            SPDLOG_WARN("[ServerService] connection_refused");
            break;
        case asio::error::bad_descriptor:
            SPDLOG_ERROR("[ServerService] bad_descriptor");
            assert(false);
            stop();
            break;
        default:
            SPDLOG_ERROR("[ServerService] 알 수 없는 에러: {}", error.value());
            assert(false);
            stop();
            break;
        }
    }

    void ServerService::close()
    {
        assert(!m_running.load());

        SPDLOG_INFO("[ServerService] 서비스 닫기");

        m_stopSignals.cancel();
        if (m_acceptor.is_open())
        {
            m_acceptor.cancel();
            m_acceptor.close();
        }

        // 서비스 이벤트 큐에 닫기 이벤트 추가
        ServiceEventPtr event = std::make_shared<CloseServiceEvent>();
        m_eventQueue.push(std::move(event));
    }

    ClientService::ClientService(asio::io_context& ioContext, const ResolveTarget& resolveTarget)
        : Service(ioContext)
        , m_resolveTarget(resolveTarget)
        , m_resolver(ioContext)
        , m_socket(ioContext)
    {}

    ClientServicePtr ClientService::createInstance(asio::io_context& ioContext, const ResolveTarget& resolveTarget)
    {
        auto service = std::make_shared<ClientService>(ioContext, resolveTarget);
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

        SPDLOG_INFO("[ClientService] 서비스 시작");

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

        SPDLOG_INFO("[ClientService] 서비스 중지");

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
                SPDLOG_INFO("[ClientService] 엔드포인트: {}:{}", endpoint.address().to_string(), endpoint.port());
            }

            m_resolveResults = results;
            asyncConnect();
        }
        else
        {
            handleError(error);
        }
    }

    void ClientService::asyncConnect()
    {
        if (!m_running.load())
        {
            return;
        }

        asio::async_connect(
            m_socket, m_resolveResults,
            asio::bind_executor(
                m_strand,
                [this, self = shared_from_this()]
                (const asio::error_code& error, const asio::ip::tcp::endpoint& endpoint)
                {
                    onConnected(error);
                }));
    }

    void ClientService::onConnected(const asio::error_code& error)
    {
        if (!error)
        {
            SPDLOG_INFO("[ClientService] 서버 연결: {}:{}", m_resolveTarget.host, m_resolveTarget.service);

            // 이벤트 큐에 서버 연결 이벤트 추가
            ServiceEventPtr event = std::make_shared<ConnectServiceEvent>(std::move(m_socket));
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
            SPDLOG_WARN("[ClientService] operation_aborted");
            break;
        case asio::error::host_not_found:
            SPDLOG_ERROR("[ClientService] host_not_found");
            stop();
            break;
        case asio::error::service_not_found:
            SPDLOG_ERROR("[ClientService] service_not_found");
            stop();
            break;
        case asio::error::connection_refused:
            SPDLOG_ERROR("[ClientService] connection_refused");
            stop();
            break;
        case asio::error::timed_out:
            SPDLOG_ERROR("[ClientService] timed_out");
            stop();
            break;
        case asio::error::network_unreachable:
            SPDLOG_ERROR("[ClientService] network_unreachable");
            stop();
            break;
        case asio::error::connection_aborted:
            SPDLOG_ERROR("[ClientService] connection_aborted");
            stop();
            break;
        case asio::error::already_connected:
            SPDLOG_ERROR("[ClientService] already_connected");
            assert(false);
            stop();
            break;
        case asio::error::bad_descriptor:
            SPDLOG_ERROR("[ClientService] bad_descriptor");
            assert(false);
            stop();
            break;
        default:
            SPDLOG_ERROR("[ClientService] 알 수 없는 에러: {}", error.value());
            assert(false);
            stop();
            break;
        }
    }

    void ClientService::close()
    {
        assert(!m_running.load());

        SPDLOG_INFO("[ClientService] 서비스 닫기");

        m_stopSignals.cancel();
        m_resolver.cancel();
        if (m_socket.is_open())
        {
            m_socket.cancel();
            m_socket.close();
        }

        // 서비스 이벤트 큐에 닫기 이벤트 추가
        ServiceEventPtr event = std::make_shared<CloseServiceEvent>();
        m_eventQueue.push(std::move(event));
    }
}
