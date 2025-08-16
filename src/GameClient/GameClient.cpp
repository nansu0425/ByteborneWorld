#include "GameClient.h"
#include <spdlog/spdlog.h>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#endif

GameClient::GameClient()
    : m_running(false)
    , m_shape(50.f)
    , m_clearColor(sf::Color::Black)
    , m_messageSerializer(m_sendBufferManager)
{    
    // 매니저들 초기화
    m_fontManager = std::make_unique<FontManager>();
    m_inputManager = std::make_unique<KoreanInputManager>();
    m_chatWindow = std::make_unique<ChatWindow>(*m_fontManager, *m_inputManager);
    
    // 네트워크 초기화
    initializeNetwork();
}

GameClient::~GameClient()
{
    if (m_running.load())
    {
        stop();
    }
}

void GameClient::start()
{
    if (m_running.exchange(true))
    {
        // 이미 실행 중인 경우 아무 작업도 하지 않음
        return;
    }

    spdlog::info("[GameClient] 게임 클라이언트 시작");

    m_mainThread = std::thread([this]()
                               {
                                   initialize();
                                   run();
                                   close();    // 네트워크 정리 작업 (UI가 있을 때)
                                   cleanup();  // UI 정리 작업 (마지막에)
                               });
    
    // 네트워크 I/O 스레드 풀 시작
    m_ioThreadPool.run();
    
    // 서버 연결 시작
    m_clientService->start();
}

void GameClient::stop()
{
    if (!m_running.exchange(false))
    {
        // 이미 중지 상태이므로 아무 작업도 하지 않음
        return;
    }

    spdlog::info("[GameClient] 게임 클라이언트 중지");

    if (m_window && m_window->isOpen())
    {
        m_window->close();
    }
    
    // 서비스 중지 (새로운 연결 시도 중단)
    m_clientService->stop();
}

void GameClient::join()
{
    if (m_mainThread.joinable())
    {
        m_mainThread.join();
    }
    
    // 네트워크 I/O 스레드 풀 종료 대기
    m_ioThreadPool.join();
    
    spdlog::info("[GameClient] 게임 클라이언트 종료");
}

void GameClient::sendChatMessage(const std::string& message)
{
    if (!m_connected.load() || m_serverSessionId == 0)
    {
        spdlog::warn("[GameClient] 서버에 연결되지 않음. 메시지를 보낼 수 없습니다.");
        return;
    }

    // 낙관적 UI를 위한 client_message_id 및 클라 보낸 시각(ms)
    const uint64_t clientMessageId = m_nextClientMessageId.fetch_add(1);
    const int64_t clientSentAtMs = [](){
        using namespace std::chrono; return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    }();

    // 채팅창에 즉시 임시 메시지 추가
    if (m_chatWindow)
    {
        m_chatWindow->addLocalPendingMessage(m_chatWindow->getUsername() + " (나)", message, clientMessageId);
    }
    
    // 채팅 메시지 생성 및 전송 (프로토콜 필드 채우기)
    proto::C2S_Chat chat;
    chat.set_sender_name(m_chatWindow ? m_chatWindow->getUsername() : ""); // 서버는 신뢰하지 않음
    chat.set_content(message);
    chat.set_client_message_id(clientMessageId);
    chat.set_client_sent_at_ms(clientSentAtMs);
    
    net::SendBufferChunkPtr chunk = m_messageSerializer.serializeToSendBuffer(chat);
    if (m_sessionManager.send(m_serverSessionId, chunk))
    {
        spdlog::debug("[GameClient] 채팅 메시지 전송: {} (cid={})", message, clientMessageId);
    }
    else
    {
        spdlog::error("[GameClient] 채팅 메시지 전송 실패: {}", message);
    }
}

void GameClient::initialize()
{
    printVersionInfo();
    initializeWindow();
    initializeImGui();
    
    // 매니저들 초기화
    m_inputManager->initialize(m_window->getSystemHandle());
    m_fontManager->initializeKoreanFont();
    
    // 채팅 윈도우의 메시지 전송 콜백 설정
    m_chatWindow->setSendMessageCallback([this](const std::string& message) {
        sendChatMessage(message);
    });
    
    initializeTestObjects();
}

void GameClient::run()
{
    constexpr auto TickInterval = std::chrono::milliseconds(16); // 60 FPS
    
    while (m_window->isOpen() && m_running.load())
    {
        auto start = std::chrono::steady_clock::now();
        
        // 네트워크 이벤트 처리
        processServiceEvents();
        processSessionEvents();
        processMessages();
        m_timer.update();
        
        // 게임 루프
        processEvents();
        updateImGui();
        renderImGuiWindows();
        renderSFML();
        
        // 다음 프레임까지 대기
        auto end = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        if (elapsed < TickInterval)
        {
            std::this_thread::sleep_for(TickInterval - elapsed);
        }
    }
}

void GameClient::cleanup()
{
    if (m_window)
    {
        ImGui::SFML::Shutdown();
        m_window.reset();
    }

    spdlog::info("[GameClient] GameClient cleanup completed successfully!");
}

void GameClient::close()
{
    // 실행 중이 아닌 상태에서 close가 호출돼야 한다
    assert(m_running.load() == false);

    spdlog::info("[GameClient] 네트워크 정리 작업 시작");
    
    // 연결 상태 업데이트
    m_connected.store(false);
    m_serverSessionId = 0;
    
    // 채팅 윈도우에 종료 상태 알림 (UI 정리 전이므로 안전하게 접근 가능)
    if (m_chatWindow)
    {
        m_chatWindow->setConnectionStatus(false);
        m_chatWindow->addChatMessage("시스템", "네트워크 연결을 정리 중입니다...");
    }

    // 모든 세션 중지
    m_sessionManager.stopAllSessions();

    // 모든 세션이 제거될 때까지 대기
    while (!m_sessionManager.isEmpty())
    {
        processSessionEvents();
        std::this_thread::yield();
    }

    // I/O 스레드가 처리할 핸들러가 없으면 스레드가 종료되도록 설정
    m_ioThreadPool.reset();
    
    // 네트워크 정리 완료 메시지 (UI 정리 전 마지막 메시지)
    if (m_chatWindow)
    {
        m_chatWindow->addChatMessage("시스템", "네트워크 정리 완료. UI를 종료합니다.");
    }
    
    spdlog::info("[GameClient] 네트워크 정리 작업 완료");
}

void GameClient::initializeNetwork()
{
    // ClientService 생성 (localhost:12345에 연결)
    m_clientService = net::ClientService::createInstance(
        m_ioThreadPool.getContext(), 
        m_serviceEventQueue, 
        net::ResolveTarget{"localhost", "12345"}, 
        1);  // 연결 개수는 1개
    
    // 메시지 핸들러 등록
    registerMessageHandlers();
    
    spdlog::info("[GameClient] 네트워크 초기화 완료");
}

void GameClient::printVersionInfo()
{
    spdlog::info("[GameClient] === Version Information ===");
    spdlog::info("[GameClient] ImGui Version: {}", IMGUI_VERSION);
    spdlog::info("[GameClient] SFML Version: {}.{}.{}",
                 SFML_VERSION_MAJOR, SFML_VERSION_MINOR, SFML_VERSION_PATCH);
    spdlog::info("[GameClient] ===========================");
}

void GameClient::initializeWindow()
{
    m_window = std::make_unique<sf::RenderWindow>(sf::VideoMode(800, 600), "ImGui-SFML Test");
    m_window->setFramerateLimit(60);
}

void GameClient::initializeImGui()
{
    ImGui::SFML::Init(*m_window);
    
    // ImGui IO 설정
    ImGuiIO& io = ImGui::GetIO();
    
    // 한글 입력을 위한 설정
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    
#ifdef _WIN32
    // Windows IME 지원 활성화
    io.ImeWindowHandle = m_window->getSystemHandle();
#endif

    // UTF-8 지원 설정
    io.ConfigInputTrickleEventQueue = true;
    io.ConfigInputTextCursorBlink = true;
    
    spdlog::info("[GameClient] ImGui-SFML initialized successfully with Korean support!");
}

void GameClient::initializeTestObjects()
{
    m_shape.setFillColor(sf::Color::Green);
    m_shape.setPosition(100.f, 100.f);
}

void GameClient::processEvents()
{
    sf::Event event;
    while (m_window->pollEvent(event))
    {
        ImGui::SFML::ProcessEvent(*m_window, event);

        if (event.type == sf::Event::Closed)
        {
            handleWindowClose();
        }
        
        // 키보드 이벤트가 발생했을 때 한영 상태 업데이트
        if (event.type == sf::Event::KeyPressed || event.type == sf::Event::KeyReleased)
        {
            m_inputManager->updateInputState();
        }
    }
    
    // 매 프레임마다 한영 상태 업데이트 (Alt+한/영 키 감지용)
    m_inputManager->updateInputState();
}

void GameClient::updateImGui()
{
    ImGui::SFML::Update(*m_window, m_deltaClock.restart());
}

void GameClient::renderImGuiWindows()
{
    m_chatWindow->render();
    renderMainMenuBar();
}

void GameClient::renderSFML()
{
    m_window->clear(m_clearColor);
    m_window->draw(m_shape);

    ImGui::SFML::Render(*m_window);
    m_window->display();
}

void GameClient::renderMainMenuBar()
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Exit"))
            {
                handleWindowClose();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Windows"))
        {
            bool chatVisible = m_chatWindow->isVisible();
            if (ImGui::MenuItem("Chat Window", nullptr, &chatVisible))
            {
                m_chatWindow->setVisible(chatVisible);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help"))
        {
            if (ImGui::MenuItem("About"))
            {
                spdlog::info("[GameClient] ImGui {} + SFML {}.{} + ImGui-SFML 2.6",
                             IMGUI_VERSION, SFML_VERSION_MAJOR, SFML_VERSION_MINOR);
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

void GameClient::handleWindowClose()
{
    m_window->close();
    stop();
}

void GameClient::moveCircleRandomly()
{
    m_shape.setPosition(
        static_cast<float>(rand() % 700),
        static_cast<float>(rand() % 500)
    );
}

void GameClient::processServiceEvents()
{
    net::ServiceEventPtr event;
    while (m_serviceEventQueue.pop(event))
    {
        switch (event->type)
        {
        case net::ServiceEventType::Close:
            handleServiceEvent(*static_cast<net::ServiceCloseEvent*>(event.get()));
            break;
        case net::ServiceEventType::Connect:
            handleServiceEvent(*static_cast<net::ServiceConnectEvent*>(event.get()));
            break;
        default:
            spdlog::error("[GameClient] 알 수 없는 서비스 이벤트 타입: {}", static_cast<int>(event->type));
            break;
        }
    }
}

void GameClient::handleServiceEvent(net::ServiceCloseEvent& event)
{
    spdlog::info("[GameClient] 서버 연결이 종료되었습니다.");
    m_connected.store(false);
    m_serverSessionId = 0;
    
    // 채팅 윈도우에 연결 상태 업데이트
    if (m_chatWindow)
    {
        m_chatWindow->setConnectionStatus(false);
        m_chatWindow->addChatMessage("시스템", "서버 연결이 종료되었습니다.");
    }
}

void GameClient::handleServiceEvent(net::ServiceConnectEvent& event)
{
    if (!m_running.load())
    {
        spdlog::debug("[GameClient] 클라이언트가 실행 중이 아닙니다. 서버 연결 이벤트를 건너뜁니다.");
        return;
    }

    auto session = net::Session::createInstance(std::move(event.socket), m_sessionEventQueue);
    m_sessionManager.addSession(session);
    session->start();
    
    m_serverSessionId = session->getSessionId();
    m_connected.store(true);
    
    spdlog::info("[GameClient] 서버에 연결되었습니다. Session ID: {}", m_serverSessionId);
    
    // 채팅 윈도우에 연결 상태 업데이트
    if (m_chatWindow)
    {
        m_chatWindow->setConnectionStatus(true);
        m_chatWindow->addChatMessage("시스템", "서버에 성공적으로 연결되었습니다!");
    }
}

void GameClient::processSessionEvents()
{
    net::SessionEventPtr event;
    while (m_sessionEventQueue.pop(event))
    {
        switch (event->type)
        {
        case net::SessionEventType::Close:
            handleSessionEvent(*static_cast<net::SessionCloseEvent*>(event.get()));
            break;
        case net::SessionEventType::Receive:
            handleSessionEvent(*static_cast<net::SessionReceiveEvent*>(event.get()));
            break;
        default:
            spdlog::error("[GameClient] 알 수 없는 세션 이벤트 타입: {}", static_cast<int>(event->type));
            break;
        }
    }
}

void GameClient::handleSessionEvent(net::SessionCloseEvent& event)
{
    spdlog::info("[GameClient] 세션 {} 연결이 종료되었습니다.", event.sessionId);
    m_sessionManager.removeSession(event.sessionId);
    
    if (event.sessionId == m_serverSessionId)
    {
        m_connected.store(false);
        m_serverSessionId = 0;
        
        // 채팅 윈도우에 연결 상태 업데이트
        if (m_chatWindow)
        {
            m_chatWindow->setConnectionStatus(false);
            m_chatWindow->addChatMessage("시스템", "서버와의 연결이 끊어졌습니다.");
        }
    }
}

void GameClient::handleSessionEvent(net::SessionReceiveEvent& event)
{
    if (!m_running.load())
    {
        spdlog::debug("[GameClient] 클라이언트가 실행 중이 아닙니다. 수신 이벤트를 건너뜁니다.");
        return;
    }

    auto session = m_sessionManager.findSession(event.sessionId);
    if (!session)
    {
        spdlog::error("[GameClient] 세션 {}을 찾을 수 없습니다.", event.sessionId);
        return;
    }

    net::PacketView packetView;
    while (session->getFrontPacket(packetView))
    {
        // 패킷의 페이로드를 메시지로 파싱하여 큐에 추가
        m_messageQueue.push(event.sessionId, packetView);

        // 수신 버퍼에서 패킷 제거
        session->popFrontPacket();
    }

    // 세션에서 다시 비동기 수신 시작
    session->receive();
}

void GameClient::processMessages()
{
    while (m_running.load() && !m_messageQueue.isEmpty())
    {
        m_messageDispatcher.dispatch(m_messageQueue.front());
        m_messageQueue.pop();
    }
}

void GameClient::registerMessageHandlers()
{
    m_messageDispatcher.registerHandler(
        proto::MessageType::S2C_Chat,
        [this](net::SessionId sessionId, const proto::MessagePtr& message)
        {
            handleMessage(sessionId, *std::static_pointer_cast<proto::S2C_Chat>(message));
        });
}

void GameClient::handleMessage(net::SessionId sessionId, const proto::S2C_Chat& message)
{
    spdlog::info("[GameClient] Session {}: S2C_Chat 수신: {} (sid={}, smid={}, cmid={})",
                 sessionId, message.content(), message.sender_session_id(),
                 message.server_message_id(), message.client_message_id());
    
    // 서버가 보낸 권위 정보 기반으로 UI에 표시/보정
    if (m_chatWindow)
    {
        // 먼저 내 pending과 매칭 시도
        if (message.client_message_id() != 0 &&
            m_chatWindow->ackPendingMessage(message.client_message_id(),
                                            message.sender_name(),
                                            message.content(),
                                            message.server_message_id(),
                                            message.server_sent_at_ms()))
        {
            return; // 보정 완료
        }

        // 매칭 실패 → 다른 유저 또는 이전 세션의 메시지. 신규 추가
        m_chatWindow->addAuthoritativeMessage(message.sender_name(),
                                              message.content(),
                                              message.server_message_id(),
                                              message.server_sent_at_ms(),
                                              message.client_message_id());
    }
}
