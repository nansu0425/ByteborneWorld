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

    using ServerIoServicePtr = std::shared_ptr<class ServerIoService>;

    class ServerIoService
        : public IoService
    {
    public:
        static ServerIoServicePtr createInstance(uint16_t port, size_t ioThreadCount, IoEventQueue& ioEventQueue);

        virtual void start() override;
        virtual void stop() override;
        virtual void join() override;

        ServerIoServicePtr getInstance() { return std::static_pointer_cast<ServerIoService>(shared_from_this()); }

    private:
        ServerIoService(uint16_t port, size_t ioThreadCount, IoEventQueue& ioEventQueue);

    private:
        void asyncAccept();
        void onAccepted(const asio::error_code& error, asio::ip::tcp::socket socket);

    private:
        asio::ip::tcp::acceptor m_acceptor;
    };

    using ClientIoServicePtr = std::shared_ptr<class ClientIoService>;

    class ClientIoService
        : public IoService
    {
    public:
        static ClientIoServicePtr createInstance(const std::string& host, uint16_t port, size_t ioThreadCount, IoEventQueue& ioEventQueue);

        virtual void start() override;
        virtual void stop() override;
        virtual void join() override;

        ClientIoServicePtr getInstance() { return std::static_pointer_cast<ClientIoService>(shared_from_this()); }

    private:
        ClientIoService(const std::string& host, uint16_t port, size_t ioThreadCount, IoEventQueue& ioEventQueue);

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
