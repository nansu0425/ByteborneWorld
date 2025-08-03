#pragma once

#include "FontManager.h"
#include "KoreanInputManager.h"
#include <string>
#include <vector>

class ChatWindow
{
public:
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
    
    // 채팅 데이터
    std::vector<ChatMessage> m_chatMessages;
    std::string m_chatInputText;
    std::string m_usernameText;
    
    // 내부 함수들
    void renderToolbar();
    void renderMessageArea();
    void renderInputArea();
    void renderInputStatusIndicator();
    
    void sendChatMessage();
    void addChatMessage(const std::string& sender, const std::string& message);
    void scrollChatToBottom();
    void addTestMessages();
    
    std::string getCurrentTimestamp() const;
};
