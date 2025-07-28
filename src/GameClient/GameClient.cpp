#include "GameClient.h"
#include <cstdlib>

GameClient::GameClient()
    : m_running(false)
    , m_shape(50.f)
    , m_clearColor(sf::Color::Black)
    , m_showDemoWindow(true)
    , m_showTestWindow(true)
    , m_testFloat(0.0f)
    , m_testCounter(0)
    , m_colorEdit{0.0f, 0.0f, 0.0f, 1.0f}
    , m_textBuffer{"Hello, World!"}
{}

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
