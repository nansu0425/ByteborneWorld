#pragma once

#include <asio.hpp>
#include "Queue.h"
#include "Thread.h"

namespace net
{
    class SessionManager;

    class Service
        : public std::enable_shared_from_this<Service>
    {
    public:
        Service(asio::io_context& ioContext);
        virtual ~Service() = default;

        virtual void start() = 0;
        virtual void stop() = 0;

        ServiceEventPtr popEvent() { return m_eventQueue.pop(); }
        bool isEventQueueEmpty() { return m_eventQueue.isEmpty(); }

    protected:
        virtual void handleError(const asio::error_code& error) = 0;
        virtual void close() = 0;

        void asyncWaitForStopSignals();

    protected:
        std::atomic<bool> m_running = false;
        asio::strand<asio::io_context::executor_type> m_strand;
        asio::signal_set m_stopSignals;
        ServiceEventQueue m_eventQueue;
    };

    using ServerServicePtr = std::shared_ptr<class ServerService>;

    class ServerService
        : public Service
    {
    public:
        ServerService(asio::io_context& ioContext, uint16_t port);

        static ServerServicePtr createInstance(asio::io_context& ioContext, uint16_t port);
        ServerServicePtr getInstance() { return std::static_pointer_cast<ServerService>(shared_from_this()); }

        virtual void start() override;
        virtual void stop() override;

    private:
        void asyncAccept();
        void onAccepted(const asio::error_code& error, asio::ip::tcp::socket&& socket);

        virtual void handleError(const asio::error_code& error) override;
        virtual void close() override;

    private:
        asio::ip::tcp::acceptor m_acceptor;
    };

    using ClientServicePtr = std::shared_ptr<class ClientService>;

    struct ResolveTarget
    {
        std::string host;
        std::string service;
    };

    class ClientService
        : public Service
    {
    public:
        ClientService(asio::io_context& ioContext, const ResolveTarget& resolveTarget, size_t connectCount);

        static ClientServicePtr createInstance(asio::io_context& ioContext, const ResolveTarget& resolveTarget, size_t connectCount);
        ClientServicePtr getInstance() { return std::static_pointer_cast<ClientService>(shared_from_this()); }

        virtual void start() override;
        virtual void stop() override;

    private:
        void asyncResolve();
        void onResolved(const asio::error_code& error, const asio::ip::tcp::resolver::results_type& results);
        void asyncConnect(size_t socketIndex);
        void onConnected(const asio::error_code& error, size_t socketIndex);

        virtual void handleError(const asio::error_code& error) override;
        virtual void close() override;

    private:
        ResolveTarget m_resolveTarget;
        asio::ip::tcp::resolver m_resolver;
        asio::ip::tcp::resolver::results_type m_resolveResults;
        std::vector<asio::ip::tcp::socket> m_sockets;
    };
}
