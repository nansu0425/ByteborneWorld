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
            std::memmove(m_buffer.data(), getReadPtr(), getDataSize());
            m_writeOffset -= m_readOffset;
            m_readOffset = 0;
        }
    }
}
