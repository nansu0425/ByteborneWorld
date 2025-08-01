﻿#pragma once

#include <asio.hpp>
#include <deque>
#include <memory>
#include "Core/LockQueue.h"
#include "Buffer.h"

namespace net
{
    using SessionPtr = std::shared_ptr<class Session>;
    using SessionId = int64_t;
    using SessionEventQueue = core::LockQueue<std::shared_ptr<struct SessionEvent>>;

    struct PacketView;

    class Session
        : public std::enable_shared_from_this<Session>
    {
    public:
        Session(SessionId sessionId, asio::ip::tcp::socket&& socket, SessionEventQueue& eventQueue);
        ~Session();

        static SessionPtr createInstance(asio::ip::tcp::socket&& socket, SessionEventQueue& eventQueue);

        void start();
        void stop();

        void receive();
        void send(const SendBufferChunkPtr& chunk);

        bool getFrontPacket(PacketView& view) const;
        void popFrontPacket();

        bool isRunning() const { return m_running.load(); }
        SessionId getSessionId() const { return m_sessionId; }
        ReceiveBuffer& getReceiveBuffer() { return m_receiveBuffer; }

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
        std::deque<SendBufferChunkPtr> m_sendQueue;
        ReceiveBuffer m_receiveBuffer;
    };

    class SessionManager
    {
    public:
        bool send(SessionId sessionId, const SendBufferChunkPtr& chunk);
        void broadcast(const SendBufferChunkPtr& chunk);
        void broadcast(const std::vector<SessionId>& sessionIds, const SendBufferChunkPtr& chunk);

        void stopAllSessions();

        void addSession(const SessionPtr& session);
        void removeSession(SessionId sessionId);
        void removeSession(const SessionPtr& session);
        SessionPtr findSession(SessionId sessionId) const;

        bool isEmpty() const { return m_sessions.empty(); }
        bool hasSession(SessionId sessionId) const { return m_sessions.find(sessionId) != m_sessions.end(); }

    private:
        std::unordered_map<SessionId, SessionPtr> m_sessions;
    };
}
