#include "Queue.h"
#include "Factory.h"
#include "Network/Packet.h"

namespace proto
{
    void MessageQueue::push(net::SessionId sessionId, const net::PacketView& packetView)
    {
        assert(packetView.isValid());

        // 메시지 큐 엔트리 생성
        MessageQueueEntry entry;
        entry.sessionId = sessionId;
        entry.messageType = static_cast<MessageType>(packetView.header->id);
        entry.message = MessageFactory::createMessage(entry.messageType);

        // 패킷의 페이로드를 메시지로 파싱
        const void* payload = packetView.payload;
        const int payloadSize = packetView.header->size - sizeof(net::PacketHeader);
        const bool parsingResult = entry.message->ParseFromArray(payload, payloadSize);
        assert(parsingResult);

        // 파싱이 성공하면 큐에 추가
        m_queue.emplace_back(std::move(entry));
    }

    void MessageQueue::pop()
    {
        assert(m_queue.empty() == false);

        m_queue.pop_front();
    }

    const MessageQueueEntry& MessageQueue::front() const
    {
        assert(m_queue.empty() == false);

        return m_queue.front();
    }

    bool MessageQueue::isEmpty() const
    {
        return m_queue.empty();
    }
}
