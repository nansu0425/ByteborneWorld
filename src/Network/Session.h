#pragma once

#include <asio.hpp>
#include <deque>
#include <memory>

namespace net
{
    using SessionPtr = std::shared_ptr<class Session>;
    using SessionId = int64_t;

    class SessionEventQueue;

    class Session
        : public std::enable_shared_from_this<Session>
    {
    public:
        using ReceiveBuffer = std::array<uint8_t, 4096>;

    public:
        Session(SessionId sessionId, asio::ip::tcp::socket&& socket, SessionEventQueue& eventQueue);
        ~Session();

        static SessionPtr createInstance(asio::ip::tcp::socket&& socket, SessionEventQueue& eventQueue);

        void start();
        void stop();

        void receive();
        void send(std::vector<uint8_t> data);

        bool isRunning() const { return m_running.load(); }
        SessionId getSessionId() const { return m_sessionId; }
        const ReceiveBuffer& getReceiveBuffer() const { return m_receiveBuffer; }

    private:
        void asyncRead();
        void onRead(const asio::error_code& error, size_t bytesRead);
        void asyncWrite();
        void onWritten(const asio::error_code& error, size_t bytesWritten);

        void handleError(const asio::error_code& error);
        void close();

    private:
        std::atomic<bool> m_running;
        SessionId m_sessionId;
        asio::ip::tcp::socket m_socket;
        SessionEventQueue& m_eventQueue;
        asio::strand<asio::ip::tcp::socket::executor_type> m_strand;
        std::deque<std::vector<uint8_t>> m_sendQueue;
        ReceiveBuffer m_receiveBuffer = {};
    };

    class SessionManager
    {
    public:
        void broadcast(const std::vector<uint8_t>& data);
        void stopAllSessions();

        void addSession(const SessionPtr& session);
        void removeSession(SessionId sessionId);
        void removeSession(const SessionPtr& session);
        SessionPtr findSession(SessionId sessionId) const;

        bool isEmpty() const { return m_sessions.empty(); }

    private:
        std::unordered_map<SessionId, SessionPtr> m_sessions;
    };
}
