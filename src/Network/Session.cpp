#include "Pch.h"  
#include "Session.h"
#include "Queue.h"

namespace net  
{
    SessionPtr Session::createInstance(asio::ip::tcp::socket socket, IoEventQueue& eventQueue)
    {
        static std::atomic<SessionId> s_nextSessionId = 1;

        auto session = std::make_shared<Session>(s_nextSessionId.fetch_add(1), std::move(socket), eventQueue);
        session->asyncReceive();

        return session;
    }

    void Session::asyncReceive()  
    {
        asio::post(m_strand,
                   [self = shared_from_this()]()
                   {
                       self->asyncRead();
                   });  
    }

    void Session::asyncSend(std::vector<uint8_t> data)  
    {
        asio::post(m_strand,
                   [self = shared_from_this(), data = std::move(data)]() mutable
                   {
                       bool writeInProgress = !self->m_sendQueue.empty();
                       self->m_sendQueue.push_back(std::move(data));

                       // 쓰기 작업이 진행 중이지 않으면 쓰기 요청
                       if (!writeInProgress)
                       {
                           self->asyncWrite();
                       }
                   });
    }


    Session::Session(SessionId sessionId, asio::ip::tcp::socket socket, IoEventQueue& eventQueue)
        : m_sessionId(sessionId)
        , m_socket(std::move(socket))
        , m_strand(asio::make_strand(m_socket.get_executor()))
        , m_eventQueue(eventQueue)
    {
        SPDLOG_INFO("[Session {}] 세션 생성", m_sessionId);
    }

    Session::~Session()
    {
        SPDLOG_INFO("[Session {}] 세션 소멸", m_sessionId);
    }

    void Session::asyncRead()  
    {
        if (m_closed)  
        {
            return;  
        }

        m_socket.async_read_some(asio::buffer(m_receiveBuffer),
                                 asio::bind_executor(m_strand,
                                                     [self = shared_from_this()]
                                                     (const asio::error_code& error, size_t bytesRead)
                                                     {
                                                         self->onRead(error, bytesRead);
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

        // 수신 이벤트를 이벤트 큐에 추가
        IoEventPtr event = std::make_shared<IoEvent>();
        event->session = shared_from_this();
        event->type = IoEventType::Receive;
        m_eventQueue.push(std::move(event));
    }

    void Session::asyncWrite()
    {
        if (m_closed)  
        {
            return;  
        }

        asio::async_write(m_socket,
                          asio::buffer(m_sendQueue.front()),
                          asio::bind_executor(m_strand,
                                              [self = shared_from_this()]
                                              (const asio::error_code& error, size_t bytesWritten)
                                              {
                                                  self->onWritten(error, bytesWritten);
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
            close();
            break;
        case asio::error::connection_aborted:
            SPDLOG_ERROR("[Session {}] connection_aborted", m_sessionId);
            close();
            break;
        case asio::error::timed_out:
            SPDLOG_ERROR("[Session {}] timed_out", m_sessionId);
            close();
            break;
        case asio::error::not_connected:
            SPDLOG_ERROR("[Session {}] not_connected", m_sessionId);
            assert(m_closed);
            break;
        case asio::error::eof:
            SPDLOG_INFO("[Session {}] eof", m_sessionId);
            close();
            break;
        case asio::error::bad_descriptor:
            SPDLOG_ERROR("[Session {}] bad_descriptor", m_sessionId);
            assert(m_closed);
            break;
        default:
            SPDLOG_ERROR("[Session {}] 알 수 없는 에러: {}", m_sessionId, error.value());
            assert(false);
            break;
        }
    }

    void Session::close()  
    {
        if (!m_closed)  
        {
            m_closed = true;
            SPDLOG_INFO("[Session {}] 세션 종료", m_sessionId);

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

            // 이벤트 큐에 세션 종료 이벤트 추가
            IoEventPtr event = std::make_shared<IoEvent>();
            event->session = shared_from_this();
            event->type = IoEventType::Disconnect;
            m_eventQueue.push(std::move(event));
        }  
    }

    void SessionManager::addSession(const SessionPtr& session)  
    {  
        m_sessions[session->getSessionId()] = session;
        SPDLOG_INFO("[SessionManager] 세션 추가: {}", session->getSessionId());
    }

    void SessionManager::removeSession(SessionId sessionId)  
    {  
        m_sessions.erase(sessionId);  
    }

    void SessionManager::removeSession(const SessionPtr& session)  
    {  
        m_sessions.erase(session->getSessionId());
        SPDLOG_INFO("[SessionManager] 세션 제거: {}", session->getSessionId());
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

    void SessionManager::broadcast(const std::vector<uint8_t>& data)  
    {  
        for (const auto& pair : m_sessions)  
        {  
            pair.second->asyncSend(data);  
        }  
    }
}
