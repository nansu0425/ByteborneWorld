#pragma once

#include <asio.hpp>
#include "Queue.h"
#include "Thread.h"

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
        IoService(IoEventQueue& ioEventQueue);
        virtual ~IoService() = default;

        virtual void start(size_t ioThreadCount) = 0;
        virtual void stop() = 0;
        virtual void join() = 0;

    protected:
        virtual void handleError(const asio::error_code& error) = 0;

    protected:
        std::atomic<IoServiceState> m_state;
        IoThreadPool m_ioThreadPool;
        IoEventQueue& m_ioEventQueue;
    };

    using ServerIoServicePtr = std::shared_ptr<class ServerIoService>;

    class ServerIoService
        : public IoService
    {
    public:
        ServerIoService(uint16_t port, IoEventQueue& ioEventQueue);

        static ServerIoServicePtr createInstance(uint16_t port, IoEventQueue& ioEventQueue);
        ServerIoServicePtr getInstance() { return std::static_pointer_cast<ServerIoService>(shared_from_this()); }

        virtual void start(size_t ioThreadCount = std::thread::hardware_concurrency()) override;
        virtual void stop() override;
        virtual void join() override;

    private:
        void asyncAccept();
        void onAccepted(const asio::error_code& error, asio::ip::tcp::socket socket);

        virtual void handleError(const asio::error_code& error) override;

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
        ClientIoService(const ResolveTarget& resolveTarget, IoEventQueue& ioEventQueue);

        static ClientIoServicePtr createInstance(const ResolveTarget& resolveTarget, IoEventQueue& ioEventQueue);
        ClientIoServicePtr getInstance() { return std::static_pointer_cast<ClientIoService>(shared_from_this()); }

        virtual void start(size_t ioThreadCount = std::thread::hardware_concurrency()) override;
        virtual void stop() override;
        virtual void join() override;

    private:
        void asyncResolve();
        void onResolved(const asio::error_code& error, const asio::ip::tcp::resolver::results_type& results);
        void asyncConnect();
        void onConnected(const asio::error_code& error);

        virtual void handleError(const asio::error_code& error) override;

    private:
        asio::ip::tcp::socket m_socket;
        asio::ip::tcp::resolver m_resolver;
        asio::ip::tcp::resolver::results_type m_resolveResults;
        ResolveTarget m_resolveTarget;
    };
}
