#pragma once

#include "Session.h"

namespace net
{
    enum class SessionEventType
    {
        None,
        Close,
        Receive,
    };

    struct SessionEvent
    {
        SessionEventType type = SessionEventType::None;
        SessionId sessionId = 0;

        SessionEvent(SessionEventType type, SessionId sessionId)
            : type(type)
            , sessionId(sessionId)
        {}
    };

    using SessionEventPtr = std::shared_ptr<SessionEvent>;

    struct CloseSessionEvent
        : public SessionEvent
    {
        CloseSessionEvent(SessionId sessionId)
            : SessionEvent(SessionEventType::Close, sessionId)
        {}
    };

    struct ReceiveSessionEvent
        : public SessionEvent
    {
        ReceiveSessionEvent(SessionId sessionId)
            : SessionEvent(SessionEventType::Receive, sessionId)
        {}
    };

    enum class ServiceEventType
    {
        None,
        Close,
        Accept,
        Connect,
    };

    struct ServiceEvent
    {
        ServiceEventType type = ServiceEventType::None;

        ServiceEvent(ServiceEventType type)
            : type(type)
        {}
    };

    using ServiceEventPtr = std::shared_ptr<ServiceEvent>;

    struct CloseServiceEvent
        : public ServiceEvent
    {
        CloseServiceEvent()
            : ServiceEvent(ServiceEventType::Close)
        {}
    };

    struct AcceptServiceEvent
        : public ServiceEvent
    {
        asio::ip::tcp::socket socket;

        AcceptServiceEvent(asio::ip::tcp::socket&& socket)
            : ServiceEvent(ServiceEventType::Accept)
            , socket(std::move(socket))
        {}
    };

    struct ConnectServiceEvent
        : public ServiceEvent
    {
        asio::ip::tcp::socket socket;

        ConnectServiceEvent(asio::ip::tcp::socket&& socket)
            : ServiceEvent(ServiceEventType::Connect)
            , socket(std::move(socket))
        {}
    };
}
