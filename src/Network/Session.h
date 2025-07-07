#pragma once

#include <asio.hpp>
#include <deque>

namespace net
{
    using SessionPtr = std::shared_ptr<class Session>;
    using SessionId = int64_t;

    class IoEventQueue;

    class Session
        : std::enable_shared_from_this<Session>
    {
    public:
        using ReceiveBuffer = std::array<uint8_t, 4096>;

    public:
        static SessionPtr createInstance(asio::ip::tcp::socket socket, IoEventQueue& eventQueue);

        void asyncReceive();
        void asyncSend(std::vector<uint8_t> data);

        SessionId getSessionId() const { return m_sessionId; }
        const ReceiveBuffer& getReceiveBuffer() const { return m_receiveBuffer; }

    private:
        Session(SessionId sessionId, asio::ip::tcp::socket socket, IoEventQueue& eventQueue);

        void asyncRead(const SessionPtr& self);
        void onRead(const SessionPtr& self, const asio::error_code& error, size_t bytesRead);
        void asyncWrite(const SessionPtr& self);
        void onWritten(const SessionPtr& self, const asio::error_code& error, size_t bytesWritten);

    private:
        SessionId m_sessionId = 0;
        asio::ip::tcp::socket m_socket;
        asio::strand<asio::ip::tcp::socket::executor_type> m_strand;
        std::deque<std::vector<uint8_t>> m_sendQueue;
        ReceiveBuffer m_receiveBuffer = {};
        IoEventQueue& m_eventQueue;
    };

    class SessionManager
    {
    public:
        void addSession(const SessionPtr& session);
        void removeSession(SessionId sessionId);
        void removeSession(const SessionPtr& session);
        SessionPtr findSession(SessionId sessionId) const;

    private:
        std::unordered_map<SessionId, SessionPtr> m_sessions;
    };
}
