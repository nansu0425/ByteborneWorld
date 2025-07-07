#pragma once

#include "Session.h"

namespace net
{
    enum class IoEventType
    {
        None,
        Connect,
        Disconnect,
        Receive,
    };

    struct IoEvent
    {
        SessionPtr session = nullptr;
        IoEventType type = IoEventType::None;
    };

    using IoEventPtr = std::shared_ptr<IoEvent>;
}
