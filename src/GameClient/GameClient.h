#pragma once

#include "FontManager.h"
#include "KoreanInputManager.h"
#include "ChatWindow.h"

#include "imgui.h"
#include "imgui-SFML.h"
#include <SFML/Graphics.hpp>

#include <atomic>
#include <thread>
#include <memory>

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

    void processEvents();
    void updateImGui();
    void renderImGuiWindows();
    void renderSFML();

    void renderMainMenuBar();
    void handleWindowClose();
    void moveCircleRandomly();

private:
    // 실행 상태
    std::atomic<bool> m_running{false};
    std::thread m_mainThread;

    // SFML 객체들
    std::unique_ptr<sf::RenderWindow> m_window;
    sf::Clock m_deltaClock;
    sf::CircleShape m_shape;
    sf::Color m_clearColor;

    // 매니저들
    std::unique_ptr<FontManager> m_fontManager;
    std::unique_ptr<KoreanInputManager> m_inputManager;
    std::unique_ptr<ChatWindow> m_chatWindow;
};
