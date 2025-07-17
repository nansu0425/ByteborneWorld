#pragma once

namespace net
{
    using PacketId = uint16_t;
    using PacketSize = uint16_t;

#pragma pack(push, 1) // 1바이트 정렬로 패킹
    struct PacketHeader
    {
        PacketSize size; // 패킷 크기 (헤더 포함)
        PacketId id;   // 패킷 식별 ID
    };
#pragma pack(pop) // 원래 정렬로 되돌리기
}
