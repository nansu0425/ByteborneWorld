#pragma once

#include "FontManager.h"
#include "KoreanInputManager.h"
#include "ChatWindow.h"

#include "imgui.h"
#include "imgui-SFML.h"
#include <SFML/Graphics.hpp>

#include "Core/Timer.h"
#include "Network/Thread.h"
#include "Network/Service.h"
#include "Network/Session.h"
#include "Network/Buffer.h"
#include "Network/Event.h"
#include "Protocol/Queue.h"
#include "Protocol/Dispatcher.h"
#include "Protocol/Serializer.h"
#include "Protocol/Type.h"

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

    // 채팅 메시지 전송 기능 추가
    void sendChatMessage(const std::string& message);

private:
    void initialize();
    void run();
    void cleanup();
    void close();  // 네트워크 정리 작업 메서드 추가

    void printVersionInfo();
    void initializeWindow();
    void initializeImGui();
    void initializeTestObjects();
    
    // 네트워크 관련 초기화 추가
    void initializeNetwork();

    void processEvents();
    void updateImGui();
    void renderImGuiWindows();
    void renderSFML();

    void renderMainMenuBar();
    void handleWindowClose();
    void moveCircleRandomly();
    
    // 네트워크 이벤트 처리 메서드 추가
    void processServiceEvents();
    void handleServiceEvent(net::ServiceCloseEvent& event);
    void handleServiceEvent(net::ServiceConnectEvent& event);
    
    void processSessionEvents();
    void handleSessionEvent(net::SessionCloseEvent& event);
    void handleSessionEvent(net::SessionReceiveEvent& event);
    
    void processMessages();
    void registerMessageHandlers();
    void handleMessage(net::SessionId sessionId, const proto::S2C_Chat& message);

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
    
    // 네트워크 통신 컴포넌트들 추가
    core::Timer m_timer;
    net::IoThreadPool m_ioThreadPool;
    net::ServiceEventQueue m_serviceEventQueue;
    net::ClientServicePtr m_clientService;
    net::SessionEventQueue m_sessionEventQueue;
    net::SessionManager m_sessionManager;
    net::SendBufferManager m_sendBufferManager;
    proto::MessageQueue m_messageQueue;
    proto::MessageDispatcher m_messageDispatcher;
    proto::MessageSerializer m_messageSerializer;
    
    // 연결 상태 관리
    std::atomic<bool> m_connected{false};
    net::SessionId m_serverSessionId{0};
};
