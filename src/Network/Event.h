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

    struct SessionCloseEvent
        : public SessionEvent
    {
        SessionCloseEvent(SessionId sessionId)
            : SessionEvent(SessionEventType::Close, sessionId)
        {}
    };

    struct SessionReceiveEvent
        : public SessionEvent
    {
        SessionReceiveEvent(SessionId sessionId)
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

    struct ServiceCloseEvent
        : public ServiceEvent
    {
        ServiceCloseEvent()
            : ServiceEvent(ServiceEventType::Close)
        {}
    };

    struct ServiceAcceptEvent
        : public ServiceEvent
    {
        asio::ip::tcp::socket socket;

        ServiceAcceptEvent(asio::ip::tcp::socket&& socket)
            : ServiceEvent(ServiceEventType::Accept)
            , socket(std::move(socket))
        {}
    };

    struct ServiceConnectEvent
        : public ServiceEvent
    {
        asio::ip::tcp::socket socket;

        ServiceConnectEvent(asio::ip::tcp::socket&& socket)
            : ServiceEvent(ServiceEventType::Connect)
            , socket(std::move(socket))
        {}
    };
}
