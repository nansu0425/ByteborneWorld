#include "GameClient.h"
#include <cstdlib>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <locale>
#include <codecvt>

#ifdef _WIN32
#include <windows.h>
#include <imm.h>
#pragma comment(lib, "imm32.lib")
#endif

GameClient::GameClient()
    : m_running(false)
    , m_shape(50.f)
    , m_clearColor(sf::Color::Black)
    , m_showChatWindow(true)
    , m_autoScroll(true)
    , m_scrollToBottom(false)
    , m_koreanFont(nullptr)
    , m_defaultFont(nullptr)
    , m_koreanInputEnabled(false)
    , m_chatInputText("")
    , m_usernameText("플레이어")
{
    // UTF-8 로케일 설정
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    std::locale::global(std::locale(""));
#endif
    
    // 환영 메시지 추가
    addChatMessage("시스템", "채팅창에 오신 것을 환영합니다!");
    addChatMessage("시스템", "한글 입력이 완벽하게 지원됩니다.");
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
    setupKoreanInput();
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

void GameClient::setupKoreanInput()
{
#ifdef _WIN32
    // Windows에서 한글 입력 활성화
    m_koreanInputEnabled = true;
    spdlog::info("[GameClient] Korean input support enabled on Windows");
#else
    m_koreanInputEnabled = true;
    spdlog::info("[GameClient] Korean input support enabled");
#endif
}

void GameClient::initializeKoreanFont()
{
    ImGuiIO& io = ImGui::GetIO();
    
    // 확장된 한글 범위 설정 (모든 한글 문자 포함)
    static const ImWchar korean_ranges[] = {
        0x0020, 0x00FF, // Basic Latin + Latin Supplement
        0x0100, 0x017F, // Latin Extended-A
        0x0180, 0x024F, // Latin Extended-B
        0x1E00, 0x1EFF, // Latin Extended Additional
        0x2000, 0x206F, // General Punctuation
        0x20A0, 0x20CF, // Currency Symbols
        0x2100, 0x214F, // Letterlike Symbols
        0x2150, 0x218F, // Number Forms
        0x2190, 0x21FF, // Arrows
        0x2200, 0x22FF, // Mathematical Operators
        0x2300, 0x23FF, // Miscellaneous Technical
        0x2460, 0x24FF, // Enclosed Alphanumerics
        0x2500, 0x257F, // Box Drawing
        0x2580, 0x259F, // Block Elements
        0x25A0, 0x25FF, // Geometric Shapes
        0x2600, 0x26FF, // Miscellaneous Symbols
        0x3000, 0x303F, // CJK Symbols and Punctuation
        0x3040, 0x309F, // Hiragana
        0x30A0, 0x30FF, // Katakana
        0x3100, 0x312F, // Bopomofo
        0x3130, 0x318F, // Hangul Compatibility Jamo
        0x3190, 0x319F, // Kanbun
        0x31A0, 0x31BF, // Bopomofo Extended
        0x31F0, 0x31FF, // Katakana Phonetic Extensions
        0x3200, 0x32FF, // Enclosed CJK Letters and Months
        0x3300, 0x33FF, // CJK Compatibility
        0x3400, 0x4DBF, // CJK Unified Ideographs Extension A
        0x4E00, 0x9FFF, // CJK Unified Ideographs
        0xA960, 0xA97F, // Hangul Jamo Extended-A
        0xAC00, 0xD7AF, // Hangul Syllables (완전한 범위)
        0xD7B0, 0xD7FF, // Hangul Jamo Extended-B
        0xF900, 0xFAFF, // CJK Compatibility Ideographs
        0xFE10, 0xFE1F, // Vertical Forms
        0xFE30, 0xFE4F, // CJK Compatibility Forms
        0xFE50, 0xFE6F, // Small Form Variants
        0xFF00, 0xFFEF, // Halfwidth and Fullwidth Forms
        0,
    };
    
    // 시스템 폰트 경로들 시도 (더 많은 폰트 추가)
    std::vector<std::string> font_paths = {
        "C:/Windows/Fonts/malgun.ttf",     // 맑은 고딕
        "C:/Windows/Fonts/malgunbd.ttf",   // 맑은 고딕 Bold
        "C:/Windows/Fonts/NanumGothic.ttf", // 나눔고딕
        "C:/Windows/Fonts/gulim.ttc",      // 굴림
        "C:/Windows/Fonts/batang.ttc",     // 바탕
        "C:/Windows/Fonts/dotum.ttc",      // 돋움
        "C:/Windows/Fonts/gungsuh.ttc",    // 궁서
    };
    
    bool font_loaded = false;
    
    // 기본 폰트 먼저 로드
    m_defaultFont = io.Fonts->AddFontDefault();
    
    for (const auto& font_path : font_paths)
    {
        FILE* font_file = nullptr;
        fopen_s(&font_file, font_path.c_str(), "rb");
        
        if (font_file)
        {
            fclose(font_file);
            
            // 메인 한글 폰트 로드 (크기 16)
            m_koreanFont = io.Fonts->AddFontFromFileTTF(font_path.c_str(), 16.0f, nullptr, korean_ranges);
            
            if (m_koreanFont)
            {
                font_loaded = true;
                spdlog::info("[GameClient] Korean font loaded: {}", font_path);
                
                // 추가 크기의 폰트도 로드 (선택사항)
                ImFontConfig config;
                config.MergeMode = false;
                config.GlyphMinAdvanceX = 16.0f;
                config.GlyphMaxAdvanceX = 16.0f;
                
                // 작은 크기 폰트
                io.Fonts->AddFontFromFileTTF(font_path.c_str(), 14.0f, &config, korean_ranges);
                // 큰 크기 폰트
                io.Fonts->AddFontFromFileTTF(font_path.c_str(), 18.0f, &config, korean_ranges);
                
                break;
            }
        }
    }
    
    if (!font_loaded)
    {
        spdlog::warn("[GameClient] Could not load Korean font, using default font with Korean ranges");
        
        // 기본 폰트에 한글 범위 추가 시도
        ImFontConfig font_config;
        font_config.MergeMode = true;
        font_config.GlyphMinAdvanceX = 16.0f;
        
        m_koreanFont = io.Fonts->AddFontDefault(&font_config);
    }
    
    // 폰트 아틀라스 빌드
    io.Fonts->Build();
    
    // ImGui-SFML에 폰트 업데이트 알림
    ImGui::SFML::UpdateFontTexture();
    
    spdlog::info("[GameClient] Font atlas built with Korean character support");
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

    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("채팅창 - 한글 완벽 지원", &m_showChatWindow))
    {
        // 상단 툴바
        ImGui::Text("사용자명:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120);

        // 사용자명 입력 (std::string 사용)
        char username_buffer[256];
        strncpy_s(username_buffer, m_usernameText.c_str(), sizeof(username_buffer) - 1);
        username_buffer[sizeof(username_buffer) - 1] = '\0';

        if (ImGui::InputText("##username", username_buffer, sizeof(username_buffer)))
        {
            m_usernameText = std::string(username_buffer);
        }

        ImGui::SameLine();
        ImGui::Checkbox("자동 스크롤", &m_autoScroll);

        ImGui::SameLine();
        if (ImGui::Button("채팅 지우기"))
        {
            m_chatMessages.clear();
        }

        ImGui::SameLine();
        if (ImGui::Button("한글 테스트"))
        {
            addChatMessage("테스트", "안녕하세요! 한글이 잘 보이나요? 🇰🇷");
            addChatMessage("테스트", "가나다라마바사아자차카타파하");
            addChatMessage("테스트", "ㄱㄴㄷㄹㅁㅂㅅㅇㅈㅊㅋㅌㅍㅎ");
            addChatMessage("테스트", "ㅏㅑㅓㅕㅗㅛㅜㅠㅡㅣ");
        }

        ImGui::Separator();

        // 채팅 메시지 영역
        const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
        if (ImGui::BeginChild("ChatMessages", ImVec2(0, -footer_height_to_reserve), false, ImGuiWindowFlags_HorizontalScrollbar))
        {
            for (const auto& msg : m_chatMessages)
            {
                // 시간 스탬프 표시 (회색)
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "[%s]", msg.timestamp.c_str());
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

        // 채팅창이 처음 나타날 때 또는 메시지 전송 후 입력 필드에 포커스 설정
        static bool setFocusOnInput = false;
        if (ImGui::IsWindowAppearing())
        {
            setFocusOnInput = true;
        }

        ImGui::Text("메시지:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-70);

        // 포커스가 필요한 경우 설정
        if (setFocusOnInput)
        {
            ImGui::SetKeyboardFocusHere();
            setFocusOnInput = false;
        }

        // 한글 입력 지원을 위한 InputText
        char input_buffer[1024];
        strncpy_s(input_buffer, m_chatInputText.c_str(), sizeof(input_buffer) - 1);
        input_buffer[sizeof(input_buffer) - 1] = '\0';

        bool enter_pressed = ImGui::InputText("##chatinput", input_buffer, sizeof(input_buffer),
                                              ImGuiInputTextFlags_EnterReturnsTrue);

        // 입력 내용 업데이트
        m_chatInputText = std::string(input_buffer);

        ImGui::SameLine();
        bool send_button = ImGui::Button("전송");

        // 한글 입력 상태 표시
        ImGui::SameLine();
        updateKoreanInputState();
        if (isKoreanInputActive())
        {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "[한]");
        }
        else
        {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[영]");
        }

        // Enter 키 또는 전송 버튼 클릭 시 메시지 전송
        if (enter_pressed || send_button)
        {
            sendChatMessage();
            setFocusOnInput = true; // 다음 프레임에서 포커스 재설정
        }

        // 도움말 표시
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                           "💡 팁: 한글/영어 전환은 Alt+한/영 키 또는 Ctrl+Space를 사용하세요.");
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
    if (m_chatInputText.empty()) return;
    
    // 공백만 있는 메시지도 전송하지 않음
    std::string trimmed = m_chatInputText;
    trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r"));
    trimmed.erase(trimmed.find_last_not_of(" \t\n\r") + 1);
    
    if (trimmed.empty()) return;
    
    // 메시지를 채팅 목록에 추가
    addChatMessage(m_usernameText, m_chatInputText);
    
    // 입력 텍스트 클리어
    m_chatInputText.clear();
    
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
    
    // 메시지 추가 로그 (UTF-8 지원)
    spdlog::info("[Chat] {}: {}", sender, message);
}

void GameClient::scrollChatToBottom()
{
    m_scrollToBottom = true;
}

bool GameClient::isKoreanInputActive() const
{
#ifdef _WIN32
    // Windows에서 현재 입력 언어 확인
    HKL hkl = GetKeyboardLayout(0);
    LANGID langId = LOWORD(hkl);
    return PRIMARYLANGID(langId) == LANG_KOREAN;
#else
    // 다른 플랫폼에서는 기본적으로 true 반환
    return m_koreanInputEnabled;
#endif
}

void GameClient::updateKoreanInputState()
{
#ifdef _WIN32
    // Windows에서 IME 상태 업데이트
    static bool lastKoreanState = false;
    bool currentKoreanState = isKoreanInputActive();
    
    if (lastKoreanState != currentKoreanState)
    {
        lastKoreanState = currentKoreanState;
        if (currentKoreanState)
        {
            spdlog::debug("[Input] Korean input mode activated");
        }
        else
        {
            spdlog::debug("[Input] English input mode activated");
        }
    }
#endif
}
