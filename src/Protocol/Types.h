#pragma once

#include <cstdint>

#include "Chat.pb.h"

namespace proto
{
    enum class MessageType : uint16_t
    {
        S2C_Chat = 1000,
        C2S_Chat = 2000,
    };
}
