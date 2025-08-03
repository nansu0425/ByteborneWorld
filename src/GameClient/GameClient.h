#pragma once

#include "Core/Context.h"

#include "imgui.h"
#include "imgui-SFML.h"

#include <SFML/Graphics.hpp>

#include <atomic>
#include <thread>
#include <chrono>
#include <vector>
#include <string>

class GameClient
{
public:
    GameClient();
    ~GameClient();

    // 복사/이동 생성자 및 할당 연산자 삭제
    GameClient(const GameClient&) = delete;
    GameClient& operator=(const GameClient&) = delete;
    GameClient(GameClient&&) = delete;
    GameClient& operator=(GameClient&&) = delete;

    void start();
    void stop();
    void join();

private:
    void initialize();
    void run();
    void cleanup();

    void printVersionInfo();
    void initializeWindow();
    void initializeImGui();
    void initializeTestObjects();
    void initializeKoreanFont();
    void setupKoreanInput();

    void processEvents();
    void updateImGui();
    void renderImGuiWindows();
    void renderSFML();

    void renderChatWindow();
    void renderMainMenuBar();

    void handleWindowClose();
    void moveCircleRandomly();
    
    // 채팅 관련 함수들
    void sendChatMessage();
    void addChatMessage(const std::string& sender, const std::string& message);
    void scrollChatToBottom();
    
    // 한글 입력 지원 함수들
    bool isKoreanInputActive() const;
    void updateKoreanInputState();

private:
    // 실행 상태
    std::atomic<bool> m_running{false};
    std::thread m_mainThread;

    // SFML 객체들
    std::unique_ptr<sf::RenderWindow> m_window;
    sf::Clock m_deltaClock;
    sf::CircleShape m_shape;
    sf::Color m_clearColor;

    // ImGui 상태 변수들
    bool m_showChatWindow;
    
    // 폰트 관련
    ImFont* m_koreanFont;
    ImFont* m_defaultFont;
    
    // 한글 입력 관련
    bool m_koreanInputEnabled;
    std::string m_lastInputText;
    
    // 채팅 관련 변수들
    struct ChatMessage {
        std::string sender;
        std::string message;
        std::string timestamp;
    };
    
    std::vector<ChatMessage> m_chatMessages;
    std::string m_chatInputText;  // string으로 변경하여 UTF-8 지원
    std::string m_usernameText;   // string으로 변경하여 UTF-8 지원
    bool m_autoScroll;
    bool m_scrollToBottom;
};
