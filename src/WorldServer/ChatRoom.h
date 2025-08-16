#pragma once

#include <unordered_map>
#include <unordered_set>
#include <atomic>
#include <cstdint>
#include <string>

#include "Network/Session.h"
#include "Protocol/Dispatcher.h"
#include "Protocol/Serializer.h"
#include "Protocol/Type.h"

namespace world
{
    class ChatRoom
    {
    public:
        ChatRoom(net::SessionManager& sessionManager, proto::MessageSerializer& serializer);

        // 접속/종료 알림
        void onClientAccepted(net::SessionId sessionId);
        void onClientClosed(net::SessionId sessionId);

        // 디스패처에 채팅 핸들러 등록
        void registerMessageHandlers(proto::MessageDispatcher& dispatcher);

    private:
        void handleChat(net::SessionId sessionId, const proto::C2S_Chat& message);
        static int64_t NowMs();

    private:
        std::atomic<uint64_t> m_nextMessageId{1};
        std::unordered_set<net::SessionId> m_activeSessions;
        std::unordered_map<net::SessionId, std::string> m_sessionNames;

        net::SessionManager& m_sessionManager;
        proto::MessageSerializer& m_serializer;
    };
}
