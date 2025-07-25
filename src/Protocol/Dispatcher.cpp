#include "Dispatcher.h"
#include <spdlog/spdlog.h>

namespace proto
{
    void MessageDispatcher::registerHandler(MessageType messageType, MessageHandler handler)
    {
        m_handlers[messageType] = std::move(handler);
    }

    void MessageDispatcher::unregisterHandler(MessageType messageType)
    {
        m_handlers.erase(messageType);
    }

    void MessageDispatcher::dispatch(const MessageQueueEntry& entry)
    {
        auto it = m_handlers.find(entry.messageType);
        if (it != m_handlers.end())
        {
            it->second(entry.sessionId, entry.message);
        }
        else
        {
            spdlog::error("[MessageDispatcher] 등록되지 않은 메시지 타입: {}", static_cast<int>(entry.messageType));
        }
    }

    bool MessageDispatcher::hasHandler(MessageType messageType) const
    {
        return m_handlers.find(messageType) != m_handlers.end();
    }
}
