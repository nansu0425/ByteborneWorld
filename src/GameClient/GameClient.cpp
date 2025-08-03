#include "GameClient.h"
#include <cstdlib>
#include <ctime>
#include <sstream>
#include <iomanip>

GameClient::GameClient()
    : m_running(false)
    , m_shape(50.f)
    , m_clearColor(sf::Color::Black)
    , m_showDemoWindow(true)
    , m_showTestWindow(true)
    , m_showChatWindow(true)
    , m_testFloat(0.0f)
    , m_testCounter(0)
    , m_colorEdit{0.0f, 0.0f, 0.0f, 1.0f}
    , m_textBuffer{"Hello, World!"}
    , m_chatInputBuffer{""}
    , m_usernameBuffer{"Player"}
    , m_autoScroll(true)
    , m_scrollToBottom(false)
{
    // 환영 메시지 추가
    addChatMessage("System", "채팅창에 오신 것을 환영합니다!");
    addChatMessage("System", "메시지를 입력하고 Enter 키를 눌러보세요.");
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

    m_mainThread = std::thread([this]()
                               {
                                   initialize();
                                   run();
                                   cleanup();
                               });
}

void GameClient::stop()
{
    if (!m_running.exchange(false))
    {
        // 이미 중지 상태이므로 아무 작업도 하지 않음
        return;
    }

    if (m_window && m_window->isOpen())
    {
        m_window->close();
    }
}

void GameClient::join()
{
    if (m_mainThread.joinable())
    {
        m_mainThread.join();
    }
}

void GameClient::initialize()
{
    printVersionInfo();
    initializeWindow();
    initializeImGui();
    initializeTestObjects();
}

void GameClient::run()
{
    while (m_window->isOpen() && m_running.load())
    {
        processEvents();
        updateImGui();
        renderImGuiWindows();
        renderSFML();
    }
}

void GameClient::cleanup()
{
    if (m_window)
    {
        ImGui::SFML::Shutdown();
        m_window.reset();
    }

    spdlog::info("[GameClient] ImGui-SFML test completed successfully!");
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
    spdlog::info("[GameClient] ImGui-SFML initialized successfully!");
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
    }
}

void GameClient::updateImGui()
{
    ImGui::SFML::Update(*m_window, m_deltaClock.restart());
}

void GameClient::renderImGuiWindows()
{
    renderDemoWindow();
    renderTestWindow();
    renderChatWindow();
    renderMainMenuBar();
}

void GameClient::renderSFML()
{
    m_window->clear(m_clearColor);
    m_window->draw(m_shape);

    ImGui::SFML::Render(*m_window);
    m_window->display();
}

void GameClient::renderDemoWindow()
{
    if (m_showDemoWindow)
    {
        ImGui::ShowDemoWindow(&m_showDemoWindow);
    }
}

void GameClient::renderTestWindow()
{
    if (m_showTestWindow)
    {
        ImGui::Begin("ImGui-SFML Test Window", &m_showTestWindow);

        // 기본 텍스트
        ImGui::Text("ImGui-SFML is working!");
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
                    1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

        ImGui::Separator();

        // 버튼 테스트
        if (ImGui::Button("Test Button"))
        {
            m_testCounter++;
            spdlog::debug("[GameClient] Button clicked! Count: {}", m_testCounter);
        }
        ImGui::SameLine();
        ImGui::Text("counter = %d", m_testCounter);

        // 슬라이더 테스트
        ImGui::SliderFloat("Test Float", &m_testFloat, 0.0f, 1.0f);

        // 컬러 에디터 테스트
        if (ImGui::ColorEdit3("Clear Color", m_colorEdit))
        {
            m_clearColor = sf::Color(
                static_cast<sf::Uint8>(m_colorEdit[0] * 255),
                static_cast<sf::Uint8>(m_colorEdit[1] * 255),
                static_cast<sf::Uint8>(m_colorEdit[2] * 255)
            );
        }

        // 체크박스 테스트
        ImGui::Checkbox("Show Demo Window", &m_showDemoWindow);

        ImGui::Separator();

        // SFML 상호작용 테스트
        ImGui::Text("SFML Integration Test:");
        if (ImGui::Button("Move Circle"))
        {
            moveCircleRandomly();
        }

        sf::Vector2f pos = m_shape.getPosition();
        ImGui::Text("Circle Position: (%.1f, %.1f)", pos.x, pos.y);

        // 입력 테스트
        ImGui::InputText("Text Input", m_textBuffer, sizeof(m_textBuffer));

        ImGui::End();
    }
}

void GameClient::renderChatWindow()
{
    if (!m_showChatWindow) return;

    ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("채팅창", &m_showChatWindow))
    {
        // 상단 툴바
        ImGui::Text("사용자명:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100);
        ImGui::InputText("##username", m_usernameBuffer, sizeof(m_usernameBuffer));
        
        ImGui::SameLine();
        ImGui::Checkbox("자동 스크롤", &m_autoScroll);
        
        ImGui::SameLine();
        if (ImGui::Button("채팅 지우기"))
        {
            m_chatMessages.clear();
        }

        ImGui::Separator();

        // 채팅 메시지 영역
        const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
        if (ImGui::BeginChild("ChatMessages", ImVec2(0, -footer_height_to_reserve), false, ImGuiWindowFlags_HorizontalScrollbar))
        {
            for (const auto& msg : m_chatMessages)
            {
                // 시간 스탬프 표시 (회색)
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[%s]", msg.timestamp.c_str());
                ImGui::SameLine();
                
                // 발신자 이름 표시 (노란색)
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "%s:", msg.sender.c_str());
                ImGui::SameLine();
                
                // 메시지 내용 표시 (흰색)
                ImGui::TextWrapped("%s", msg.message.c_str());
            }

            // 자동 스크롤 처리
            if (m_scrollToBottom || (m_autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()))
            {
                ImGui::SetScrollHereY(1.0f);
            }
            m_scrollToBottom = false;
        }
        ImGui::EndChild();

        // 메시지 입력 영역
        ImGui::Separator();
        
        // 입력 필드에 포커스 설정
        if (ImGui::IsWindowAppearing())
        {
            ImGui::SetKeyboardFocusHere();
        }
        
        ImGui::SetNextItemWidth(-70);
        bool enter_pressed = ImGui::InputText("##chatinput", m_chatInputBuffer, sizeof(m_chatInputBuffer), 
                                            ImGuiInputTextFlags_EnterReturnsTrue);
        
        ImGui::SameLine();
        bool send_button = ImGui::Button("전송");
        
        // Enter 키 또는 전송 버튼 클릭 시 메시지 전송
        if (enter_pressed || send_button)
        {
            sendChatMessage();
            ImGui::SetKeyboardFocusHere(-1); // 입력 필드에 다시 포커스
        }
    }
    ImGui::End();
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
            ImGui::MenuItem("Demo Window", nullptr, &m_showDemoWindow);
            ImGui::MenuItem("Test Window", nullptr, &m_showTestWindow);
            ImGui::MenuItem("Chat Window", nullptr, &m_showChatWindow);
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

void GameClient::sendChatMessage()
{
    // 빈 메시지는 전송하지 않음
    if (strlen(m_chatInputBuffer) == 0) return;
    
    // 메시지를 채팅 목록에 추가
    addChatMessage(m_usernameBuffer, m_chatInputBuffer);
    
    // 입력 버퍼 클리어
    memset(m_chatInputBuffer, 0, sizeof(m_chatInputBuffer));
    
    // 하단으로 스크롤
    scrollChatToBottom();
}

void GameClient::addChatMessage(const std::string& sender, const std::string& message)
{
    ChatMessage chatMsg;
    chatMsg.sender = sender;
    chatMsg.message = message;
    
    // 현재 시간을 타임스탬프로 생성
    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%H:%M:%S");
    chatMsg.timestamp = oss.str();
    
    m_chatMessages.push_back(chatMsg);
    
    // 최대 1000개 메시지만 보관 (메모리 절약)
    if (m_chatMessages.size() > 1000)
    {
        m_chatMessages.erase(m_chatMessages.begin());
    }
}

void GameClient::scrollChatToBottom()
{
    m_scrollToBottom = true;
}
