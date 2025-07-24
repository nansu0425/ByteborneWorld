#pragma once

#include "Chat.pb.h"

namespace proto
{
    using Message = ::google::protobuf::Message;
    using MessagePtr = std::shared_ptr<Message>;

    enum class MessageType : uint16_t
    {
        None = 0,
        S2C_Chat = 1000,
        C2S_Chat = 2000,
    };
}
