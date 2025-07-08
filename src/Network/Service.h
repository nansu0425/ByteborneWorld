#pragma once

#include <asio.hpp>
#include "Queue.h"

namespace net
{
    class SessionManager;

    enum class IoServiceState
    {
        Stopped,
        Running,
        Stopping,
    };

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

    protected:
        std::atomic<IoServiceState> m_state;
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
        ServerIoService(uint16_t port, size_t ioThreadCount, IoEventQueue& ioEventQueue);

        static ServerIoServicePtr createInstance(uint16_t port, size_t ioThreadCount, IoEventQueue& ioEventQueue);

        virtual void start() override;
        virtual void stop() override;
        virtual void join() override;

        ServerIoServicePtr getInstance() { return std::static_pointer_cast<ServerIoService>(shared_from_this()); }

    private:
        void asyncAccept();
        void onAccepted(const asio::error_code& error, asio::ip::tcp::socket socket);

        void handleError(const asio::error_code& error);

    private:
        asio::ip::tcp::acceptor m_acceptor;
    };

    using ClientIoServicePtr = std::shared_ptr<class ClientIoService>;

    struct ResolveTarget
    {
        std::string host;
        std::string service;
    };

    class ClientIoService
        : public IoService
    {
    public:
        ClientIoService(const ResolveTarget& resolveTarget, size_t ioThreadCount, IoEventQueue& ioEventQueue);

        static ClientIoServicePtr createInstance(const ResolveTarget& resolveTarget, size_t ioThreadCount, IoEventQueue& ioEventQueue);

        virtual void start() override;
        virtual void stop() override;
        virtual void join() override;

        ClientIoServicePtr getInstance() { return std::static_pointer_cast<ClientIoService>(shared_from_this()); }

    private:
        void asyncResolve();
        void onResolved(const asio::error_code& error, const asio::ip::tcp::resolver::results_type& results);
        void asyncConnect();
        void onConnected(const asio::error_code& error);

        void handleError(const asio::error_code& error);

    private:
        asio::ip::tcp::socket m_socket;
        asio::ip::tcp::resolver m_resolver;
        asio::ip::tcp::resolver::results_type m_resolveResults;
        ResolveTarget m_resolveTarget;
    };
}
