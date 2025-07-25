#pragma once

#include "Chat.pb.h"
#include "Network/Packet.h"

namespace proto
{
    using Message = ::google::protobuf::Message;
    using MessagePtr = std::shared_ptr<Message>;

    enum class MessageType : net::PacketId
    {
        None = 0,
        S2C_Chat = 1000,
        C2S_Chat = 2000,
    };

    template<typename T>
    struct MessageTypeTraits;

    ////////////////////////////////////////////////////////////////////////////////////////
    // 메시지 타입에 따라 MessageTypeTraits 특수화

    template<>
    struct MessageTypeTraits<S2C_Chat>
    {
        static constexpr MessageType Value = MessageType::S2C_Chat;
    };

    template<>
    struct MessageTypeTraits<C2S_Chat>
    {
        static constexpr MessageType Value = MessageType::C2S_Chat;
    };

    ////////////////////////////////////////////////////////////////////////////////////////
}
