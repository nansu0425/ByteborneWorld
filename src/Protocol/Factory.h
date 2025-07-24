#pragma once

#include "Type.h"

#include <functional>
#include <unordered_map>

namespace proto
{
    class MessageFactory
    {
    public:
        static MessagePtr createMessage(MessageType type)
        {
            auto it = s_factory.find(type);
            if (it != s_factory.end())
            {
                return it->second();
            }

            return nullptr;
        }

    private:
        static inline std::unordered_map<MessageType, std::function<MessagePtr()>> s_factory =
        {
            { MessageType::S2C_Chat, []() { return std::make_shared<S2C_Chat>(); } },
            { MessageType::C2S_Chat, []() { return std::make_shared<C2S_Chat>(); } }
        };
    };
}
