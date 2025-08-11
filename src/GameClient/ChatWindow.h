#pragma once

#include "FontManager.h"
#include "KoreanInputManager.h"
#include <string>
#include <vector>
#include <functional>

class ChatWindow
{
public:
    using SendChatMessageCallback = std::function<void(const std::string&)>;

    ChatWindow(FontManager& fontManager, KoreanInputManager& inputManager);
    ~ChatWindow() = default;

    // 복사/이동 방지
    ChatWindow(const ChatWindow&) = delete;
    ChatWindow& operator=(const ChatWindow&) = delete;
    ChatWindow(ChatWindow&&) = delete;
    ChatWindow& operator=(ChatWindow&&) = delete;

    void render();
    
    bool isVisible() const { return m_isVisible; }
    void setVisible(bool visible) { m_isVisible = visible; }
    
    // 외부에서 메시지를 추가할 수 있는 public 메서드
    void addChatMessage(const std::string& sender, const std::string& message);
    
    // 메시지 전송 콜백 설정
    void setSendMessageCallback(SendChatMessageCallback callback) { m_sendMessageCallback = callback; }

    // 연결 상태 설정
    void setConnectionStatus(bool connected) { m_connected = connected; }

private:
    struct ChatMessage {
        std::string sender;
        std::string message;
        std::string timestamp;
    };

    FontManager& m_fontManager;
    KoreanInputManager& m_inputManager;
    
    // UI 상태
    bool m_isVisible;
    bool m_autoScroll;
    bool m_scrollToBottom;
    bool m_connected;
    
    // 채팅 데이터
    std::vector<ChatMessage> m_chatMessages;
    std::string m_chatInputText;
    std::string m_usernameText;
    
    // 메시지 전송 콜백
    SendChatMessageCallback m_sendMessageCallback;
    
    // 내부 함수들
    void renderToolbar();
    void renderMessageArea();
    void renderInputArea();
    void renderInputStatusIndicator();
    void renderConnectionStatus();
    
    void sendChatMessage();
    void scrollChatToBottom();
    void addTestMessages();
    
    std::string getCurrentTimestamp() const;
};
