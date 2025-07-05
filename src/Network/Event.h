#pragma once

#include "Session.h"

namespace net
{
    enum class SessionEventType
    {
        None,
        Connect,
        Disconnect,
        Receive,
    };

    struct SessionEvent
    {
        SessionEventType type = SessionEventType::None;
        SeesionPtr session = nullptr;
    };

    using SessionEventPtr = std::shared_ptr<SessionEvent>;
}
