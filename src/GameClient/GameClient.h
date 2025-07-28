#pragma once

#include "Core/Context.h"

#include "imgui.h"
#include "imgui-SFML.h"

#include <SFML/Graphics.hpp>

#include <atomic>
#include <thread>
#include <chrono>

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

    void renderDemoWindow();
    void renderTestWindow();
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

    // ImGui 상태 변수들
    bool m_showDemoWindow;
    bool m_showTestWindow;
    float m_testFloat;
    int m_testCounter;
    float m_colorEdit[4];
    char m_textBuffer[256];
};
