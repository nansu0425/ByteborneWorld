#include "Pch.h"  
#include "Session.h"
#include "Queue.h"

namespace net  
{
    Session::Session(SessionId sessionId, asio::ip::tcp::socket&& socket, SessionEventQueue& eventQueue)
        : m_running(false)
        , m_sessionId(sessionId)
        , m_socket(std::move(socket))
        , m_eventQueue(eventQueue)
        , m_strand(asio::make_strand(m_socket.get_executor()))
    {
        SPDLOG_INFO("[Session {}] 세션 생성", m_sessionId);
    }

    Session::~Session()
    {
        SPDLOG_INFO("[Session {}] 세션 소멸", m_sessionId);
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

        SPDLOG_INFO("[Session {}] 세션 시작", m_sessionId);

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

        SPDLOG_INFO("[Session {}] 세션 중지", m_sessionId);

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

    void Session::send(std::vector<uint8_t> data)  
    {
        if (!m_running.load())  
        {  
            return;  
        }

        asio::post(
            m_strand,
            [this, self = shared_from_this(), data = std::move(data)]() mutable
            {
                bool writeInProgress = !m_sendQueue.empty();
                m_sendQueue.push_back(std::move(data));

                // 쓰기 작업이 진행 중이지 않으면 쓰기 요청
                if (!writeInProgress)
                {
                    asyncWrite();
                }
            });
    }

    void Session::asyncRead()  
    {
        if (!m_running)  
        {
            return;  
        }

        m_socket.async_read_some(
            asio::buffer(m_receiveBuffer),
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

        SPDLOG_INFO("[Session {}] bytesRead: {}", m_sessionId, bytesRead);

        // 이벤트 큐에 receive 이벤트 추가
        SessionEventPtr event = std::make_shared<ReceiveSessionEvent>(m_sessionId);
        m_eventQueue.push(std::move(event));
    }

    void Session::asyncWrite()
    {
        if (!m_running)  
        {
            return;  
        }

        asio::async_write(
            m_socket,
            asio::buffer(m_sendQueue.front()),
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

        SPDLOG_INFO("[Session {}] bytesWritten: {}", m_sessionId, bytesWritten);
        
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
            SPDLOG_WARN("[Session {}] operation_aborted", m_sessionId);
            break;
        case asio::error::connection_reset:
            SPDLOG_ERROR("[Session {}] connection_reset", m_sessionId);
            stop();
            break;
        case asio::error::connection_aborted:
            SPDLOG_ERROR("[Session {}] connection_aborted", m_sessionId);
            stop();
            break;
        case asio::error::timed_out:
            SPDLOG_ERROR("[Session {}] timed_out", m_sessionId);
            stop();
            break;
        case asio::error::not_connected:
            SPDLOG_ERROR("[Session {}] not_connected", m_sessionId);
            assert(!m_running.load());
            break;
        case asio::error::eof:
            SPDLOG_INFO("[Session {}] eof", m_sessionId);
            stop();
            break;
        case asio::error::bad_descriptor:
            SPDLOG_ERROR("[Session {}] bad_descriptor", m_sessionId);
            assert(!m_running.load());
            break;
        default:
            SPDLOG_ERROR("[Session {}] 알 수 없는 에러: {}", m_sessionId, error.value());
            assert(false);
            stop();
            break;
        }
    }

    void Session::close()  
    {
        assert(!m_running.load());

        if (!m_socket.is_open())
        {
            SPDLOG_WARN("[Session {}] 소켓이 이미 닫혀 있습니다.", m_sessionId);
            return;
        }

        SPDLOG_INFO("[Session {}] 세션 닫기", m_sessionId);

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
        SessionEventPtr event = std::make_shared<CloseSessionEvent>(m_sessionId);
        m_eventQueue.push(std::move(event));
    }

    void SessionManager::broadcast(const std::vector<uint8_t>& data)
    {
        for (const auto& pair : m_sessions)
        {
            pair.second->send(data);
        }
    }

    void SessionManager::stopAllSessions()
    {
        SPDLOG_INFO("[SessionManager] 모든 세션 중지");

        for (const auto& pair : m_sessions)
        {
            pair.second->stop();
        }
    }

    void SessionManager::addSession(const SessionPtr& session)  
    {
        assert(session->isRunning() == false);

        SPDLOG_INFO("[SessionManager] 세션 추가: {}", session->getSessionId());

        m_sessions[session->getSessionId()] = session;
    }

    void SessionManager::removeSession(SessionId sessionId)  
    {
        if (m_sessions.find(sessionId) == m_sessions.end())
        {
            SPDLOG_WARN("[SessionManager] 세션 제거 실패: {} (존재하지 않는 세션 ID)", sessionId);
            return;
        }
        
        assert(m_sessions[sessionId]->isRunning() == false);

        SPDLOG_INFO("[SessionManager] 세션 제거: {}", sessionId);

        m_sessions.erase(sessionId);
    }

    void SessionManager::removeSession(const SessionPtr& session)  
    {
        if (session == nullptr)
        {
            SPDLOG_WARN("[SessionManager] 세션 제거 실패: nullptr");
            return;
        }

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
