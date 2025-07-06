#include "NetworkPch.h"
#include "Service.h"
#include "Session.h"
#include "Queue.h"

namespace net
{
    AcceptService::AcceptService(asio::io_context& io_context, uint16_t port, SessionEventQueue& eventQueue)
        : m_ioContext(io_context)
        , m_acceptor(io_context, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port))
        , m_eventQueue(eventQueue)
    {}

    void AcceptService::start()
    {
        asyncAccept();
    }

    void AcceptService::asyncAccept()
    {
        auto self = shared_from_this();
        m_acceptor.async_accept([self](const asio::error_code& error, asio::ip::tcp::socket socket)
                                {
                                    self->onAccepted(error, std::move(socket));
                                });
    }

    void AcceptService::onAccepted(const asio::error_code& error, asio::ip::tcp::socket socket)
    {
        if (!error)
        {
            SPDLOG_INFO("새로운 연결이 수락되었습니다. 소켓: {}", socket.remote_endpoint().address().to_string());

            // 이벤트 큐에 연결 이벤트 추가
            SessionEventPtr event = std::make_shared<SessionEvent>();
            event->type = SessionEventType::Connect;
            event->session = SessionManager::createSession(std::move(socket), m_eventQueue);
            m_eventQueue.push(event);
        }
        else
        {
            SPDLOG_ERROR("연결 수락 오류: {}", error.message());
        }
        // 다음 연결을 수락하기 위해 다시 호출
        asyncAccept();
    }

    ConnectService::ConnectService(asio::io_context& io_context, const std::string& host, uint16_t port, SessionEventQueue& eventQueue)
        : m_ioContext(io_context)
        , m_socket(io_context)
        , m_resolver(io_context)
        , m_eventQueue(eventQueue)
        , m_host(host)
        , m_port(port)
    {}

    void ConnectService::start()
    {
        asyncResolve();
    }

    void ConnectService::asyncResolve()
    {
        auto self = shared_from_this();
        m_resolver.async_resolve(m_host, std::to_string(m_port),
                                 [self](const asio::error_code& error, asio::ip::tcp::resolver::results_type results)
                                 {
                                     self->onResolved(error, results);
                                 });
    }

    void ConnectService::onResolved(const asio::error_code& error, asio::ip::tcp::resolver::results_type results)
    {
        if (!error)
        {
            SPDLOG_INFO("호스트 {}:{}를 성공적으로 해석했습니다.", m_host, m_port);
            m_resolvedEndpoints = results;
            asyncConnect();
        }
        else
        {
            SPDLOG_ERROR("호스트 해석 오류: {}", error.message());
        }
    }

    void ConnectService::asyncConnect()
    {
        auto self = shared_from_this();
        asio::async_connect(m_socket, m_resolvedEndpoints,
                            [self](const asio::error_code& error, const asio::ip::tcp::endpoint&)
                            {
                                self->onConnected(error);
                            });
    }

    void ConnectService::onConnected(const asio::error_code& error)
    {
        if (!error)
        {
            SPDLOG_INFO("서버 {}:{}에 성공적으로 연결되었습니다.", m_host, m_port);

            // 이벤트 큐에 연결 이벤트 추가
            SessionEventPtr event = std::make_shared<SessionEvent>();
            event->type = SessionEventType::Connect;
            event->session = SessionManager::createSession(std::move(m_socket), m_eventQueue);
            m_eventQueue.push(event);
        }
        else
        {
            SPDLOG_ERROR("연결 오류: {}", error.message());
        }
    }
}
