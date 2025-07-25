#pragma once

#include <functional>
#include <unordered_map>
#include "Queue.h"

namespace proto
{
    // 메시지 핸들러 함수 타입 정의
    using MessageHandler = std::function<void(net::SessionId, const MessagePtr&)>;

    class MessageDispatcher
    {
    public:
        // 메시지 타입에 대한 핸들러 등록
        void registerHandler(MessageType messageType, MessageHandler handler);

        // 메시지 타입에 대한 핸들러 해제
        void unregisterHandler(MessageType messageType);

        // 메시지 전달
        void dispatch(const MessageQueueEntry& entry);

        // 등록된 핸들러가 있는지 확인
        bool hasHandler(MessageType messageType) const;

    private:
        std::unordered_map<MessageType, MessageHandler> m_handlers;
    };
}
