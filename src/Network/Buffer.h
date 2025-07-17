#pragma once

#include <vector>
#include <stack>
#include <mutex>

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
        const uint8_t* getReadPtr() const { return m_buffer.data() + m_readOffset; }
        uint8_t* getWritePtr() { return m_buffer.data() + m_writeOffset; }
        const uint8_t* getWritePtr() const { return m_buffer.data() + m_writeOffset; }

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
    
    class SendBufferChunk;
    class SendBuffer;

    using SendBufferChunkPtr = std::shared_ptr<SendBufferChunk>;
    using SendBufferPtr = std::shared_ptr<SendBuffer>;

    class SendBufferChunk
        : public std::enable_shared_from_this<SendBufferChunk>
    {
    public:
        SendBufferChunk(const SendBufferPtr& owner, uint8_t* chunk, size_t openSize);
        static SendBufferChunkPtr create(const SendBufferPtr& owner, uint8_t* chunk, size_t openSize);

        void onWritten(size_t bytesWritten);
        void close();

        uint8_t* getWritePtr() { return m_chunk + m_writeOffset; }
        const uint8_t* getWritePtr() const { return m_chunk + m_writeOffset; }
        uint8_t* getReadPtr() { return m_chunk; }
        const uint8_t* getReadPtr() const { return m_chunk; }
        size_t getWrittenSize() const { return m_writeOffset; }
        size_t getUnwrittenSize() const { return m_openSize - m_writeOffset; }
        bool isClosed() const { return m_closed; }

    private:
        SendBufferPtr m_owner;
        uint8_t* m_chunk = nullptr;
        size_t m_openSize = 0;
        size_t m_writeOffset = 0;
        bool m_closed = false;
    };

    class SendBuffer
        : public std::enable_shared_from_this<SendBuffer>
    {
    public:
        static constexpr size_t DefaultSize = 4096;

        SendBuffer(size_t size = DefaultSize);
        ~SendBuffer();
        static SendBufferPtr create(size_t size = DefaultSize);

        SendBufferChunkPtr open(size_t size);
        void close(size_t bytesWritten);

        bool isClosed() const { return m_closed; }
        size_t getFreeSize() const { return m_buffer.size() - m_chunkOffset; }
        uint8_t* getChunkPtr() { return m_buffer.data() + m_chunkOffset; }

    private:
        std::vector<uint8_t> m_buffer;
        size_t m_chunkOffset = 0;
        bool m_closed = true;
    };

    class SendBufferManager
    {
    public:
        SendBufferManager();

        SendBufferChunkPtr open(size_t size);

    private:
        SendBufferPtr m_currentBuffer;
    };
}
