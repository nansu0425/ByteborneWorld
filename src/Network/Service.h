#pragma once

#include <asio.hpp>

namespace net
{
    class SessionEventQueue;

    class AcceptService
        : public std::enable_shared_from_this<AcceptService>
    {
    public:
        AcceptService(asio::io_context& io_context, uint16_t port, SessionEventQueue& eventQueue);

        void start();

    private:
        void doAccept();
        void onAccepted(const asio::error_code& error, asio::ip::tcp::socket socket);

    private:
        asio::io_context& m_ioContext;
        asio::ip::tcp::acceptor m_acceptor;
        SessionEventQueue& m_eventQueue;
    };

    class ConnectService
        : public std::enable_shared_from_this<ConnectService>
    {
    public:
        ConnectService(asio::io_context& io_context, const std::string& host, uint16_t port, SessionEventQueue& eventQueue);

        void start();

    private:
        void doResolve();
        void onResolved(const asio::error_code& error, asio::ip::tcp::resolver::results_type results);
        void doConnect();
        void onConnected(const asio::error_code& error);

    private:
        asio::io_context& m_ioContext;
        asio::ip::tcp::socket m_socket;
        asio::ip::tcp::resolver m_resolver;
        asio::ip::tcp::resolver::results_type m_resolvedEndpoints;
        SessionEventQueue& m_eventQueue;
        std::string m_host;
        uint16_t m_port;
    };
}
