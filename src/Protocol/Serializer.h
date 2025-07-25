#pragma once

#include "Type.h"
#include "Network/Buffer.h"
#include "Network/Packet.h"
#include <type_traits>

namespace proto
{
    // 메시지 타입에서 MessageType enum 값을 추출하는 템플릿
    template<typename T>
    struct MessageTypeTraits;

    ////////////////////////////////////////////////////////////////////////////////////////
    // 메시지 타입에 따라 MessageTypeTraits 특수화

    template<>
    struct MessageTypeTraits<S2C_Chat>
    {
        static constexpr MessageType value = MessageType::S2C_Chat;
    };

    template<>
    struct MessageTypeTraits<C2S_Chat>
    {
        static constexpr MessageType value = MessageType::C2S_Chat;
    };

    ////////////////////////////////////////////////////////////////////////////////////////

    class MessageSerializer
    {
    public:
        // 메시지를 SendBufferChunk에 직렬화하는 템플릿 함수
        template<typename TMessage>
        static net::SendBufferChunkPtr serialize(net::SendBufferManager& bufferManager, const TMessage& message)
        {
            static_assert(std::is_base_of_v<Message, TMessage>,
                          "TMessage must inherit from proto::Message");

            // 메시지 크기 계산
            size_t messageSize = message.ByteSizeLong();
            size_t totalSize = sizeof(net::PacketHeader) + messageSize;

            // 전송 버퍼 열기
            net::SendBufferChunkPtr chunk = bufferManager.open(totalSize);

            // 패킷 헤더 작성
            net::PacketHeader* header = reinterpret_cast<net::PacketHeader*>(chunk->getWritePtr());
            header->size = static_cast<net::PacketSize>(chunk->getUnwrittenSize());
            header->id = static_cast<net::PacketId>(MessageTypeTraits<TMessage>::value);

            // 메시지 직렬화
            message.SerializeToArray(header + 1, messageSize);

            // 쓰기 완료 처리
            chunk->onWritten(header->size);
            chunk->close();

            return chunk;
        }

        // 편의 함수: 메시지를 생성하고 직렬화까지 한 번에 처리
        template<typename TMessage, typename... Args>
        static net::SendBufferChunkPtr createAndSerialize(net::SendBufferManager& bufferManager, Args&&... args)
        {
            static_assert(std::is_base_of_v<Message, TMessage>,
                          "TMessage must inherit from proto::Message");

            TMessage message;

            // 가변 템플릿 인자를 사용하여 메시지 설정
            setMessageData(message, std::forward<Args>(args)...);
            return serialize(bufferManager, message);
        }

    private:
        ////////////////////////////////////////////////////////////////////////////////////////
        // 메시지 타입에 따라 setMessageData 오버로딩

        static void setMessageData(S2C_Chat& message, const std::string& content)
        {
            message.set_content(content);
        }

        ////////////////////////////////////////////////////////////////////////////////////////
    };
}
