#include "Pch.h"  
#include "Session.h"
#include "Queue.h"

namespace net  
{
    SeesionPtr Session::createInstance(asio::ip::tcp::socket socket, IoEventQueue& eventQueue)
    {
        std::atomic<SessionId> s_nextSessionId = 1;
        // 세션 생성 후 수신 요청
        Session* session = new Session(s_nextSessionId.fetch_add(1), std::move(socket), eventQueue);
        session->asyncReceive();

        return SeesionPtr(session);
    }

    void Session::asyncReceive()  
    {  
        auto self = shared_from_this();
        asio::post(m_strand,
                   [self]()
                   {
                       // 읽기 작업을 시작
                       self->asyncRead();
                   });  
    }

    void Session::asyncSend(const std::vector<uint8_t>& data)  
    {
        auto self = shared_from_this();
        asio::post(m_strand,
                   [self, data]()
                   {
                       bool writeInProgress = !self->m_sendQueue.empty();
                       self->m_sendQueue.push_back(data);

                       // 쓰기 작업이 진행 중이지 않으면 asyncWrite 호출
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
        auto self = shared_from_this();
        m_socket.async_read_some(asio::buffer(m_receiveBuffer),
                                 asio::bind_executor(m_strand,
                                                     [self](const asio::error_code& error, size_t bytesRead)
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
            m_eventQueue.push(event);
        }  
        else  
        {  
            SPDLOG_ERROR("읽기 오류: {}", error.value());  
        }  
    }

    void Session::asyncWrite()  
    {
        // 쓰기 큐가 비어있으면 쓰기 작업을 중단
        if (m_sendQueue.empty())
        {
            return;
        }
        auto data = std::move(m_sendQueue.front());  
        m_sendQueue.pop_front();

        auto self = shared_from_this();
        asio::async_write(m_socket,
                          asio::buffer(data),
                          asio::bind_executor(m_strand,
                                              [self](const asio::error_code& error, size_t bytesWritten)
                                              {
                                                  self->onWritten(error, bytesWritten);
                                              }));  
    }

    void Session::onWritten(const asio::error_code& error, size_t bytesWritten)
    {  
        if (!error)  
        {  
            SPDLOG_INFO("데이터를 {} 바이트 전송했습니다.", bytesWritten);  
            // 다음 데이터를 전송
            asyncWrite();  
        }  
        else  
        {  
            SPDLOG_ERROR("쓰기 오류: {}", error.value());  
        }  
    }

    void SessionManager::addSession(const SeesionPtr& session)  
    {  
        m_sessions[session->getSessionId()] = session;
    }

    void SessionManager::removeSession(SessionId sessionId)  
    {  
        m_sessions.erase(sessionId);  
    }

    void SessionManager::removeSession(const SeesionPtr& session)  
    {  
        m_sessions.erase(session->getSessionId());  
    }

    SeesionPtr SessionManager::findSession(SessionId sessionId) const  
    {  
        auto it = m_sessions.find(sessionId);  
        if (it != m_sessions.end())  
        {  
            return it->second;  
        }

        return nullptr;  
    }
}
