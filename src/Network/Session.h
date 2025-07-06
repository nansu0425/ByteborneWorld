#pragma once

#include <asio.hpp>
#include <deque>
#include <atomic>

namespace net
{
    using SeesionPtr = std::shared_ptr<class Session>;
    using SessionId = int64_t;

    class SessionEventQueue;

    class Session
        : std::enable_shared_from_this<Session>
    {
    public:
        using ReceiveBuffer = std::array<uint8_t, 4096>;

    public:
        Session(SessionId sessionId, asio::ip::tcp::socket socket, SessionEventQueue& eventQueue);

        void asyncReceive();
        void asyncSend(const std::vector<uint8_t>& data);

        SessionId getSessionId() const { return m_sessionId; }
        const ReceiveBuffer& getReceiveBuffer() const { return m_receiveBuffer; }

    private:
        void asyncRead();
        void onRead(const asio::error_code& error, size_t bytesRead);
        void asyncWrite();
        void onWritten(const asio::error_code& error, size_t bytesWritten);

    private:
        SessionId m_sessionId = 0;
        asio::ip::tcp::socket m_socket;
        asio::strand<asio::ip::tcp::socket::executor_type> m_strand;
        std::deque<std::vector<uint8_t>> m_sendQueue;
        ReceiveBuffer m_receiveBuffer = {};
        SessionEventQueue& m_eventQueue;
    };

    class SessionManager
    {
    public:
        static SeesionPtr createSession(asio::ip::tcp::socket socket, SessionEventQueue& eventQueue);

        void addSession(const SeesionPtr& session);
        void removeSession(SessionId sessionId);
        void removeSession(const SeesionPtr& session);
        SeesionPtr findSession(SessionId sessionId) const;

    private:
        std::unordered_map<SessionId, SeesionPtr> m_sessions;
    };
}
