#pragma once

#include <asio.hpp>

namespace net
{
    class SessionEventQueue;

    class ServerService
        : public std::enable_shared_from_this<ServerService>
    {
    public:
        ServerService(asio::io_context& io_context, uint16_t port, SessionEventQueue& eventQueue);

        void start();

    private:
        void asyncAccept();
        void onAccepted(const asio::error_code& error, asio::ip::tcp::socket socket);

    private:
        asio::io_context& m_ioContext;
        asio::ip::tcp::acceptor m_acceptor;
        SessionEventQueue& m_eventQueue;
    };

    class ClientService
        : public std::enable_shared_from_this<ClientService>
    {
    public:
        ClientService(asio::io_context& io_context, const std::string& host, uint16_t port, SessionEventQueue& eventQueue);

        void start();

    private:
        void asyncResolve();
        void onResolved(const asio::error_code& error, asio::ip::tcp::resolver::results_type results);
        void asyncConnect();
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
