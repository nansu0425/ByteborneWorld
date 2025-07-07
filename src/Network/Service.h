#pragma once

#include <asio.hpp>
#include "Queue.h"

namespace net
{
    class SessionManager;

    class IoService
        : public std::enable_shared_from_this<IoService>
    {
    public:
        IoService(size_t ioThreadCount, IoEventQueue& ioEventQueue);
        virtual ~IoService() = default;

        virtual void start() = 0;
        virtual void stop() = 0;
        virtual void join() = 0;

        asio::io_context& getIoContext() { return m_ioContext; }
        net::IoEventQueue& getIoEventQueue() { return m_ioEventQueue; }

    protected:
        void startIoThreads();
        void stopIoThreads();
        void joinIoThreads();

    private:
        asio::io_context m_ioContext;
        asio::executor_work_guard<asio::io_context::executor_type> m_ioWorkGuard;
        std::vector<std::thread> m_ioThreads;
        size_t m_ioThreadCount;
        net::IoEventQueue& m_ioEventQueue;
    };

    class ServerService
        : public IoService
    {
    public:
        ServerService(uint16_t port, size_t ioThreadCount, IoEventQueue& ioEventQueue);

        virtual void start() override;
        virtual void stop() override;
        virtual void join() override;

        std::shared_ptr<ServerService> getInstance() { return std::static_pointer_cast<ServerService>(shared_from_this()); }

    private:
        void asyncAccept();
        void onAccepted(const asio::error_code& error, asio::ip::tcp::socket socket);

    private:
        asio::ip::tcp::acceptor m_acceptor;
    };

    class ClientService
        : public IoService
    {
    public:
        ClientService(const std::string& host, uint16_t port, size_t ioThreadCount, IoEventQueue& ioEventQueue);

        virtual void start() override;
        virtual void stop() override;
        virtual void join() override;

        std::shared_ptr<ClientService> getInstance() { return std::static_pointer_cast<ClientService>(shared_from_this()); }

    private:
        void asyncResolve();
        void onResolved(const asio::error_code& error, asio::ip::tcp::resolver::results_type results);
        void asyncConnect();
        void onConnected(const asio::error_code& error);

    private:
        asio::ip::tcp::socket m_socket;
        asio::ip::tcp::resolver m_resolver;
        asio::ip::tcp::resolver::results_type m_resolvedEndpoints;
        std::string m_host;
        uint16_t m_port;
    };
}
