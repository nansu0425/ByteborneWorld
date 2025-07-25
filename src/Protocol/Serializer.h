#pragma once

#include "Type.h"
#include "Network/Buffer.h"
#include "Network/Packet.h"
#include <type_traits>

namespace proto
{
    class MessageSerializer
    {
    public:
        MessageSerializer(net::SendBufferManager& sendBufferManager)
            : m_sendBufferManager(sendBufferManager)
        {}

        // 메시지를 패킷 형태로 SendBuffer에 직렬화하는 템플릿 함수
        template<typename TMessage>
        net::SendBufferChunkPtr serializeToSendBuffer(const TMessage& message)
        {
            static_assert(std::is_base_of_v<Message, TMessage>,
                          "TMessage must inherit from proto::Message");

            // 메시지 크기 계산
            size_t messageSize = message.ByteSizeLong();
            size_t totalSize = sizeof(net::PacketHeader) + messageSize;

            // 전송 버퍼 열기
            net::SendBufferChunkPtr chunk = m_sendBufferManager.open(totalSize);

            // 패킷 헤더 작성
            net::PacketHeader* header = reinterpret_cast<net::PacketHeader*>(chunk->getWritePtr());
            header->size = static_cast<net::PacketSize>(chunk->getUnwrittenSize());
            header->id = static_cast<net::PacketId>(MessageTypeTraits<TMessage>::Value);

            // 메시지 직렬화
            message.SerializeToArray(header + 1, messageSize);

            // 쓰기 완료 처리
            chunk->onWritten(header->size);
            chunk->close();

            return chunk;
        }

    private:
        net::SendBufferManager& m_sendBufferManager;
    };
}
