#include "Pch.h"  
#include "Session.h"
#include "Queue.h"

namespace net  
{
    SessionPtr Session::createInstance(asio::ip::tcp::socket socket, IoEventQueue& eventQueue)
    {
        std::atomic<SessionId> s_nextSessionId = 1;
        // 세션 생성 후 수신 요청
        SessionPtr session = SessionPtr(new Session(s_nextSessionId.fetch_add(1), std::move(socket), eventQueue));
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
        SPDLOG_INFO("새로운 세션이 생성되었습니다. 소켓: {}", m_socket.remote_endpoint().address().to_string());
    }

    void Session::asyncRead()  
    {
        if (m_closed)  
        {  
            SPDLOG_WARN("세션이 이미 종료되었습니다. 세션 ID: {}", m_sessionId);
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

        SPDLOG_INFO("데이터를 {} 바이트 읽었습니다.", bytesRead);

        // 수신 이벤트를 이벤트 큐에 추가
        IoEventPtr event = std::make_shared<IoEvent>(IoEvent{shared_from_this(), IoEventType::Receive});
        m_eventQueue.push(std::move(event));
    }

    void Session::asyncWrite()
    {
        if (m_closed)  
        {  
            SPDLOG_WARN("세션이 이미 종료되었습니다. 세션 ID: {}", m_sessionId);
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

        SPDLOG_INFO("데이터를 {} 바이트 전송했습니다.", bytesWritten);
        
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
            SPDLOG_WARN("작업이 중단되었습니다. 세션 ID: {}", m_sessionId);
            break;
        case asio::error::connection_reset:
            SPDLOG_ERROR("연결이 재설정되었습니다. 세션 ID: {}", m_sessionId);
            close();
            break;
        case asio::error::connection_aborted:
            SPDLOG_ERROR("연결이 중단되었습니다. 세션 ID: {}", m_sessionId);
            close();
            break;
        case asio::error::timed_out:
            SPDLOG_ERROR("연결이 시간 초과되었습니다. 세션 ID: {}", m_sessionId);
            close();
            break;
        case asio::error::not_connected:
            SPDLOG_ERROR("소켓이 연결되지 않았습니다. 세션 ID: {}", m_sessionId);
            assert(false);
            break;
        case asio::error::eof:
            SPDLOG_INFO("세션이 EOF를 수신했습니다. 세션 ID: {}", m_sessionId);
            close();
            break;
        case asio::error::bad_descriptor:
            SPDLOG_ERROR("잘못된 디스크립터입니다. 세션 ID: {}", m_sessionId);
            assert(false);
            break;
        default:
            SPDLOG_ERROR("알 수 없는 오류가 발생했습니다. 오류 코드: {}, 세션 ID: {}", error.value(), m_sessionId);
            assert(false);
            break;
        }
    }

    void Session::close()  
    {
        if (!m_closed)  
        {
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
            m_closed = true;

            SPDLOG_INFO("세션이 종료되었습니다. 세션 ID: {}", m_sessionId);
            // 이벤트 큐에 세션 종료 이벤트 추가
            m_eventQueue.push(std::make_shared<IoEvent>(IoEvent{shared_from_this(), IoEventType::Disconnect}));
        }  
    }

    void SessionManager::addSession(const SessionPtr& session)  
    {  
        m_sessions[session->getSessionId()] = session;
    }

    void SessionManager::removeSession(SessionId sessionId)  
    {  
        m_sessions.erase(sessionId);  
    }

    void SessionManager::removeSession(const SessionPtr& session)  
    {  
        m_sessions.erase(session->getSessionId());  
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
