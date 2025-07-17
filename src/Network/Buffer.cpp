#include "Pch.h"
#include "Network/Buffer.h"

namespace net
{
    ReceiveBuffer::ReceiveBuffer(size_t size)
        : m_size(size)
    {
        m_buffer.resize(m_size * CapacityFactor);
    }

    void ReceiveBuffer::onRead(size_t bytesRead)
    {
        assert(bytesRead <= getUnreadSize());
        assert(bytesRead <= m_size);

        m_readOffset += bytesRead;
        resetOffsets();
    }

    void ReceiveBuffer::onWritten(size_t bytesWritten)
    {
        assert(bytesWritten <= getUnwrittenSize());
        assert(bytesWritten <= m_size);

        m_writeOffset += bytesWritten;
    }

    void ReceiveBuffer::resetOffsets()
    {
        assert(m_readOffset <= m_writeOffset);

        if (m_readOffset == m_writeOffset)
        {
            // 읽기와 쓰기가 모두 끝났으면 오프셋을 초기화
            m_readOffset = 0;
            m_writeOffset = 0;
        }
        else if ((0 < m_readOffset) &&
                 (getUnwrittenSize() < m_size))
        {
            // 읽기 오프셋이 0이 아니고, 쓰기 가능한 공간이 m_size보다 작으면 데이터 앞으로 이동
            std::memmove(m_buffer.data(), getReadPtr(), getUnreadSize());
            m_writeOffset -= m_readOffset;
            m_readOffset = 0;
        }
    }

    SendBufferChunk::SendBufferChunk(const SendBufferPtr& owner, uint8_t* chunk, size_t openSize)
        : m_owner(owner)
        , m_chunk(chunk)
        , m_openSize(openSize)
    {
        assert(m_owner->isClosed() == false);
        assert(m_openSize <= m_owner->getFreeSize());
    }

    SendBufferChunkPtr SendBufferChunk::create(const SendBufferPtr& owner, uint8_t* chunk, size_t openSize)
    {
        return std::make_shared<SendBufferChunk>(owner, chunk, openSize);
    }

    void SendBufferChunk::onWritten(size_t bytesWritten)
    {
        assert(m_closed == false);
        assert(bytesWritten <= getUnwrittenSize());

        m_writeOffset += bytesWritten;
    }

    void SendBufferChunk::close()
    {
        assert(m_closed == false);
        m_closed = true;

        // SendBuffer 쓰기 종료 처리
        m_owner->close(m_writeOffset);
    }

    SendBuffer::SendBuffer(size_t size)
    {
        m_buffer.resize(size);

        spdlog::debug("[SendBuffer] 버퍼 생성: {} bytes", size);
    }

    SendBuffer::~SendBuffer()
    {
        spdlog::debug("[SendBuffer] 버퍼 소멸: {} bytes, 오프셋: {}", m_buffer.size(), m_chunkOffset);
    }

    SendBufferPtr SendBuffer::create(size_t size)
    {
        return std::make_shared<SendBuffer>(size);
    }

    SendBufferChunkPtr SendBuffer::open(size_t size)
    {
        assert(isClosed());
        assert(size <= getFreeSize());

        spdlog::debug("[SendBuffer] 버퍼 열기: {} bytes, 오프셋: {}", size, m_chunkOffset);

        m_closed = false;
        auto chunk = SendBufferChunk::create(shared_from_this(), getChunkPtr(), size);

        return chunk;
    }

    void SendBuffer::close(size_t bytesWritten)
    {
        assert(isClosed() == false);
        assert(bytesWritten <= getFreeSize());

        m_chunkOffset += bytesWritten;
        m_closed = true;

        spdlog::debug("[SendBuffer] 버퍼 닫기: {} bytes, 오프셋: {}", bytesWritten, m_chunkOffset);
    }

    SendBufferManager::SendBufferManager()
    {
        m_currentBuffer = SendBuffer::create();
    }

    SendBufferChunkPtr SendBufferManager::open(size_t size)
    {
        assert(m_currentBuffer);
        assert(m_currentBuffer->isClosed());

        if (m_currentBuffer->getFreeSize() < size)
        {
            // 현재 버퍼에 충분한 공간이 없으면 새 버퍼로 교체
            m_currentBuffer = SendBuffer::create();
        }

        return m_currentBuffer->open(size);
    }
}
