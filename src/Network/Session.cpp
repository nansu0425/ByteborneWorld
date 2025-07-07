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
        if (!error)  
        {  
            SPDLOG_INFO("데이터를 {} 바이트 읽었습니다.", bytesRead);
            // 수신 이벤트를 이벤트 큐에 추가
            IoEventPtr event = std::make_shared<IoEvent>();
            event->type = IoEventType::Receive;
            event->session = shared_from_this();
            m_eventQueue.push(std::move(event));
        }  
        else  
        {  
            SPDLOG_ERROR("읽기 오류: {}", error.value());  
        }  
    }

    void Session::asyncWrite()
    {
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
        // 전송한 데이터를 큐에서 제거
        m_sendQueue.pop_front();

        if (!error)  
        {  
            SPDLOG_INFO("데이터를 {} 바이트 전송했습니다.", bytesWritten);

            // 큐에 남아있는 데이터가 있다면 다음 쓰기 요청
            if (!m_sendQueue.empty())  
            {  
                asyncWrite();
            }
        }  
        else  
        {  
            SPDLOG_ERROR("쓰기 오류: {}", error.value());  
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
