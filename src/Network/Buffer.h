#pragma once

#include <vector>

namespace net
{
    class ReceiveBuffer
    {
    public:
        static constexpr size_t DefaultSize = 4096;
        static constexpr size_t CapacityFactor = 4;

    public:
        ReceiveBuffer(size_t size = DefaultSize);

        void onRead(size_t bytesRead);
        void onWritten(size_t bytesWritten);

        uint8_t* getReadPtr() { return m_buffer.data() + m_readOffset; }
        uint8_t* getWritePtr() { return m_buffer.data() + m_writeOffset; }
        size_t getUnwrittenSize() const { return m_buffer.size() - m_writeOffset; }
        size_t getUnreadSize() const { return m_writeOffset - m_readOffset; }

    private:
        void resetOffsets();

    private:
        std::vector<uint8_t> m_buffer;
        size_t m_size;
        size_t m_readOffset = 0;
        size_t m_writeOffset = 0;
    };
}
