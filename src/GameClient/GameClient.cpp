#include "GameClient.h"
#include <cstdlib>
#include <ctime>
#include <locale>

#ifdef _WIN32
#include <windows.h>
#endif

GameClient::GameClient()
    : m_running(false)
    , m_shape(50.f)
    , m_clearColor(sf::Color::Black)
{
    // UTF-8 로케일 설정
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    std::locale::global(std::locale(""));
#endif
    
    // 매니저들 초기화
    m_fontManager = std::make_unique<FontManager>();
    m_inputManager = std::make_unique<KoreanInputManager>();
    m_chatWindow = std::make_unique<ChatWindow>(*m_fontManager, *m_inputManager);
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
    
    // 매니저들 초기화
    m_inputManager->initialize(m_window->getSystemHandle());
    m_fontManager->initializeKoreanFont();
    
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
