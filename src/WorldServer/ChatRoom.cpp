#include "ChatRoom.h"

using namespace world;

ChatRoom::ChatRoom(net::SessionManager& sessionManager, proto::MessageSerializer& serializer)
    : m_sessionManager(sessionManager)
    , m_serializer(serializer)
{
}

void ChatRoom::onClientAccepted(net::SessionId sessionId)
{
    m_activeSessions.insert(sessionId);
    // 권위 이름 부여 (간단한 규칙). 실제 서비스에선 인증 기반으로 설정.
    m_sessionNames[sessionId] = std::string("플레이어") + std::to_string(static_cast<uint64_t>(sessionId));
}

void ChatRoom::onClientClosed(net::SessionId sessionId)
{
    m_activeSessions.erase(sessionId);
    m_sessionNames.erase(sessionId);
}

void ChatRoom::registerMessageHandlers(proto::MessageDispatcher& dispatcher)
{
    dispatcher.registerHandler(
        proto::MessageType::C2S_Chat,
        [this](net::SessionId sessionId, const proto::MessagePtr& message)
        {
            handleChat(sessionId, *std::static_pointer_cast<proto::C2S_Chat>(message));
        });
}

int64_t ChatRoom::NowMs()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

void ChatRoom::handleChat(net::SessionId sessionId, const proto::C2S_Chat& message)
{
    // 서버 권위 이름 결정
    auto it = m_sessionNames.find(sessionId);
    const std::string senderName = (it != m_sessionNames.end())
        ? it->second
        : std::string("플레이어") + std::to_string(static_cast<uint64_t>(sessionId));

    // 응답 메시지 구성
    proto::S2C_Chat response;
    response.set_sender_name(senderName);
    response.set_content(message.content());
    response.set_client_message_id(message.client_message_id());
    response.set_server_message_id(m_nextMessageId.fetch_add(1));
    response.set_server_sent_at_ms(NowMs());
    response.set_sender_session_id(static_cast<uint32_t>(sessionId));

    net::SendBufferChunkPtr chunk = m_serializer.serializeToSendBuffer(response);

    // 브로드캐스트 (활성 세션 모두)
    bool anySent = false;
    for (const auto& sid : m_activeSessions)
    {
        if (m_sessionManager.send(sid, chunk))
        {
            anySent = true;
        }
        else
        {
            spdlog::warn("[ChatRoom] 세션 {}에 S2C_Chat 전송 실패", sid);
        }
    }

    if (!anySent)
    {
        spdlog::warn("[ChatRoom] 브로드캐스트 대상 세션이 없습니다.");
    }
}

