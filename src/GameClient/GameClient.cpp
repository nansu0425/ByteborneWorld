#include "GameClient.h"
#include <cstdlib>
#include <ctime>
#include <sstream>
#include <iomanip>

GameClient::GameClient()
    : m_running(false)
    , m_shape(50.f)
    , m_clearColor(sf::Color::Black)
    , m_showChatWindow(true)
    , m_autoScroll(true)
    , m_scrollToBottom(false)
    , m_koreanFont(nullptr)
{
    // 채팅 입력 버퍼 초기화
    memset(m_chatInputBuffer, 0, sizeof(m_chatInputBuffer));
    strcpy_s(m_usernameBuffer, sizeof(m_usernameBuffer), "플레이어");
    
    // 환영 메시지 추가
    addChatMessage("시스템", "채팅창에 오신 것을 환영합니다!");
    addChatMessage("시스템", "메시지를 입력하고 Enter 키를 눌러보세요.");
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
    initializeKoreanFont();
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

void GameClient::initializeKoreanFont()
{
    ImGuiIO& io = ImGui::GetIO();
    
    // 한글 범위 설정
    static const ImWchar korean_ranges[] = {
        0x0020, 0x00FF, // Basic Latin + Latin Supplement
        0x3131, 0x3163, // Korean Jamo
        0xAC00, 0xD7A3, // Korean Syllables
        0,
    };
    
    // 시스템 폰트 경로들 시도
    std::vector<std::string> font_paths = {
        "C:/Windows/Fonts/malgun.ttf",     // 맑은 고딕
        "C:/Windows/Fonts/gulim.ttc",      // 굴림
        "C:/Windows/Fonts/batang.ttc",     // 바탕
        "C:/Windows/Fonts/dotum.ttc",      // 돋움
    };
    
    bool font_loaded = false;
    
    for (const auto& font_path : font_paths)
    {
        FILE* font_file = nullptr;
        fopen_s(&font_file, font_path.c_str(), "rb");
        
        if (font_file)
        {
            fclose(font_file);
            
            // 폰트 로드 시도
            m_koreanFont = io.Fonts->AddFontFromFileTTF(font_path.c_str(), 16.0f, nullptr, korean_ranges);
            
            if (m_koreanFont)
            {
                font_loaded = true;
                spdlog::info("[GameClient] Korean font loaded: {}", font_path);
                break;
            }
        }
    }
    
    if (!font_loaded)
    {
        // 폰트 로드 실패 시 기본 폰트에 한글 범위 추가 시도
        ImFontConfig font_config;
        font_config.MergeMode = true;
        
        // 내장된 기본 폰트에 한글 범위만 추가 (한글은 여전히 표시되지 않지만 크래시 방지)
        io.Fonts->AddFontDefault(&font_config);
        
        spdlog::warn("[GameClient] Could not load Korean font, using default font");
    }
    
    // 폰트 아틀라스 빌드
    io.Fonts->Build();
    
    // ImGui-SFML에 폰트 업데이트 알림
    ImGui::SFML::UpdateFontTexture();
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

void GameClient::renderChatWindow()
{
    if (!m_showChatWindow) return;

    // 한글 폰트가 로드되었다면 푸시
    if (m_koreanFont)
    {
        ImGui::PushFont(m_koreanFont);
    }

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

    // 한글 폰트가 로드되었다면 팝
    if (m_koreanFont)
    {
        ImGui::PopFont();
    }
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
