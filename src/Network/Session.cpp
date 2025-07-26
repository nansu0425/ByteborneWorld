#include "Session.h"
#include "Packet.h"
#include "Event.h"

namespace net  
{
    Session::Session(SessionId sessionId, asio::ip::tcp::socket&& socket, SessionEventQueue& eventQueue)
        : m_running(false)
        , m_sessionId(sessionId)
        , m_socket(std::move(socket))
        , m_eventQueue(eventQueue)
        , m_strand(asio::make_strand(m_socket.get_executor()))
    {
        spdlog::debug("[Session {}] 세션 생성", m_sessionId);
    }

    Session::~Session()
    {
        spdlog::debug("[Session {}] 세션 소멸", m_sessionId);
    }

    SessionPtr Session::createInstance(asio::ip::tcp::socket&& socket, SessionEventQueue& eventQueue)
    {
        static std::atomic<SessionId> s_nextSessionId = 1;

        return std::make_shared<Session>(s_nextSessionId.fetch_add(1), std::move(socket), eventQueue);
    }

    void Session::start()
    {
        if (m_running.exchange(true))
        {
            // 이미 실행 중인 경우 아무 작업도 하지 않음
            return;
        }

        spdlog::debug("[Session {}] 세션 시작", m_sessionId);

        asio::post(
            m_strand,
            [this, self = shared_from_this()]()
            {
                // 비동기 읽기 시작
                asyncRead();
            });
    }

    void Session::stop()
    {
        if (!m_running.exchange(false))
        {
            // 이미 중지 상태이므로 아무 작업도 하지 않음
            return;
        }

        spdlog::debug("[Session {}] 세션 중지", m_sessionId);

        asio::post(
            m_strand,
            [this, self = shared_from_this()]()
            {
                close();
            });
    }

    void Session::receive()  
    {
        if (!m_running.load())  
        {  
            return;  
        }

        asio::post(
            m_strand,
            [this, self = shared_from_this()]()
            {
                asyncRead();
            });
    }

    void Session::send(const SendBufferChunkPtr& chunk)
    {
        if (!m_running.load())  
        {  
            return;  
        }

        asio::post(
            m_strand,
            [this, self = shared_from_this(), chunk = chunk]()
            {
                bool writeInProgress = !m_sendQueue.empty();
                m_sendQueue.push_back(chunk);

                // 쓰기 작업이 진행 중이지 않으면 쓰기 요청
                if (!writeInProgress)
                {
                    asyncWrite();
                }
            });
    }

    bool Session::getFrontPacket(PacketView& view) const  
    {
        if (m_receiveBuffer.getUnreadSize() < sizeof(PacketHeader))  
        {  
            return false;
        }

        const PacketHeader* header = reinterpret_cast<const PacketHeader*>(m_receiveBuffer.getReadPtr());
        if (m_receiveBuffer.getUnreadSize() < header->size)
        {  
            return false;
        }

        view.header = header;
        view.payload = m_receiveBuffer.getReadPtr() + sizeof(PacketHeader);

        return true;
    }

    void Session::popFrontPacket()
    {
        if (m_running.load() == false)  
        {  
            return;  
        }

        assert(sizeof(PacketHeader) <= m_receiveBuffer.getUnreadSize());

        const PacketHeader* header = reinterpret_cast<const PacketHeader*>(m_receiveBuffer.getReadPtr());
        assert(header->size <= m_receiveBuffer.getUnreadSize());

        // 패킷 크기만큼 읽기 오프셋 이동
        m_receiveBuffer.onRead(header->size);
    }

    void Session::asyncRead()  
    {
        if (m_running.load() == false)  
        {
            return;  
        }

        m_socket.async_read_some(
            asio::buffer(
                m_receiveBuffer.getWritePtr(),
                m_receiveBuffer.getUnwrittenSize()),
            asio::bind_executor(
                m_strand,
                [this, self = shared_from_this()]
                (const asio::error_code& error, size_t bytesRead)
                {
                    onRead(error, bytesRead);
                }));
    }

    void Session::onRead(const asio::error_code& error, size_t bytesRead)
    {
        if (error)  
        {
            handleError(error);  
            return;  
        }

        m_receiveBuffer.onWritten(bytesRead);

        // 이벤트 큐에 receive 이벤트 추가
        SessionEventPtr event = std::make_shared<SessionReceiveEvent>(m_sessionId);
        m_eventQueue.push(std::move(event));
    }

    void Session::asyncWrite()
    {
        if (!m_running.load())  
        {
            return;  
        }

        asio::async_write(
            m_socket,
            asio::buffer(
                m_sendQueue.front()->getReadPtr(),
                m_sendQueue.front()->getWrittenSize()),
            asio::bind_executor(
                m_strand,
                [this, self = shared_from_this()]
                (const asio::error_code& error, size_t bytesWritten)
                {
                    onWritten(error, bytesWritten);
                }));
    }

    void Session::onWritten(const asio::error_code& error, size_t bytesWritten)
    {
        if (error)  
        {
            handleError(error);  
            return;  
        }
        
        m_sendQueue.pop_front();
        if (!m_sendQueue.empty())
        {
            // 큐에 남아있는 데이터가 있다면 다음 쓰기 요청
            asyncWrite();
        }
    }

    void Session::handleError(const asio::error_code& error)  
    {
        switch (error.value())
        {
        case asio::error::operation_aborted:
            spdlog::debug("[Session {}] operation_aborted", m_sessionId);
            break;
        case asio::error::connection_reset:
            spdlog::debug("[Session {}] connection_reset", m_sessionId);
            stop();
            break;
        case asio::error::connection_aborted:
            spdlog::debug("[Session {}] connection_aborted", m_sessionId);
            stop();
            break;
        case asio::error::timed_out:
            spdlog::debug("[Session {}] timed_out", m_sessionId);
            stop();
            break;
        case asio::error::not_connected:
            spdlog::error("[Session {}] not_connected", m_sessionId);
            assert(!m_running.load());
            stop();
            break;
        case asio::error::eof:
            spdlog::debug("[Session {}] eof", m_sessionId);
            stop();
            break;
        case asio::error::bad_descriptor:
            spdlog::error("[Session {}] bad_descriptor", m_sessionId);
            assert(!m_running.load());
            stop();
            break;
        default:
            spdlog::error("[Session {}] 알 수 없는 에러: {}", m_sessionId, error.value());
            assert(false);
            stop();
            break;
        }
    }

    void Session::close()  
    {
        assert(!m_running.load());
        assert(m_socket.is_open());

        spdlog::debug("[Session {}] 세션 닫기", m_sessionId);

        asio::error_code error;

        // 모든 비동기 작업을 취소
        m_socket.cancel(error);
        if (error)
        {
            handleError(error);
        }

        // 송수신 기능 중지
        m_socket.shutdown(asio::ip::tcp::socket::shutdown_both, error);
        if (error)
        {
            handleError(error);
        }

        // 소켓 닫기
        m_socket.close(error);
        if (error)
        {
            handleError(error);
        }

        // 이벤트 큐에 close 이벤트 추가
        SessionEventPtr event = std::make_shared<SessionCloseEvent>(m_sessionId);
        m_eventQueue.push(std::move(event));
    }

    bool SessionManager::send(SessionId sessionId, const SendBufferChunkPtr& chunk)
    {
        auto session = findSession(sessionId);
        if (!session || !session->isRunning())
        {
            return false;
        }

        session->send(chunk);

        return true;
    }

    void SessionManager::broadcast(const SendBufferChunkPtr& chunk)
    {
        for (const auto& pair : m_sessions)
        {
            pair.second->send(chunk);
        }
    }

    void SessionManager::broadcast(const std::vector<SessionId>& sessionIds, const SendBufferChunkPtr& chunk)
    {
        for (SessionId sessionId : sessionIds)
        {
            auto session = findSession(sessionId);
            if (session && session->isRunning())
            {
                session->send(chunk);
            }
        }
    }

    void SessionManager::stopAllSessions()
    {
        for (const auto& pair : m_sessions)
        {
            pair.second->stop();
        }

        spdlog::debug("[SessionManager] 모든 세션 중지");
    }

    void SessionManager::addSession(const SessionPtr& session)  
    {
        assert(session->isRunning() == false);

        m_sessions[session->getSessionId()] = session;
        spdlog::debug("[SessionManager] 세션 추가: {}", session->getSessionId());
    }

    void SessionManager::removeSession(SessionId sessionId)  
    {
        if (hasSession(sessionId) == false)
        {
            return;
        }

        assert(m_sessions[sessionId]->isRunning() == false);

        m_sessions.erase(sessionId);
        spdlog::debug("[SessionManager] 세션 제거: {}", sessionId);
    }

    void SessionManager::removeSession(const SessionPtr& session)  
    {
        assert(session);

        removeSession(session->getSessionId());
    }

    SessionPtr SessionManager::findSession(SessionId sessionId) const  
    {  
        auto it = m_sessions.find(sessionId);  
        if (it != m_sessions.end())  
        {  
            return it->second;  
        }

        return nullptr;  
    }
}
