#pragma once

#include "FontManager.h"
#include "KoreanInputManager.h"
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>

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

    // 낙관적 UI 보조 API
    void addLocalPendingMessage(const std::string& sender, const std::string& message, uint64_t clientMessageId);
    // 서버 권위 메시지로 보정(성공 시 true)
    bool ackPendingMessage(uint64_t clientMessageId,
                           const std::string& authoritativeSender,
                           const std::string& authoritativeContent,
                           uint64_t serverMessageId,
                           int64_t serverSentAtMs);
    // 다른 유저 메시지 등, pending이 없는 경우 직접 추가
    void addAuthoritativeMessage(const std::string& sender,
                                 const std::string& content,
                                 uint64_t serverMessageId,
                                 int64_t serverSentAtMs,
                                 uint64_t clientMessageId = 0);

    // UI/입력용 사용자 이름
    const std::string& getUsername() const { return m_usernameText; }
    
    // 메시지 전송 콜백 설정
    void setSendMessageCallback(SendChatMessageCallback callback) { m_sendMessageCallback = callback; }

    // 연결 상태 설정
    void setConnectionStatus(bool connected) { m_connected = connected; }

private:
    struct ChatMessage {
        std::string sender;
        std::string message;
        std::string timestamp;
        // 낙관적 UI/권위 보정용
        uint64_t clientMessageId = 0;
        uint64_t serverMessageId = 0;
        int64_t serverSentAtMs = 0;
        bool pending = false;
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

    // 빠른 매칭용 (clientMessageId -> index)
    std::unordered_map<uint64_t, size_t> m_pendingIndex;
    
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
    static std::string formatTimestampFromMs(int64_t msSinceEpoch);
    void sortMessagesByServerOrder();
};
