#include "Core/Context.h"

#include "imgui.h"
#include "imgui-SFML.h"

#include <SFML/Graphics.hpp>

int main()
{
    // 버전 정보 출력
    std::cout << "=== Version Information ===" << std::endl;
    std::cout << "ImGui Version: " << IMGUI_VERSION << std::endl;
    std::cout << "SFML Version: " << SFML_VERSION_MAJOR << "."
        << SFML_VERSION_MINOR << "." << SFML_VERSION_PATCH << std::endl;
    std::cout << "===========================" << std::endl;

    core::AppContext::getInstance().initialize();

    // SFML 2.6.1 스타일 윈도우 생성
    sf::RenderWindow window(sf::VideoMode(800, 600), "ImGui-SFML Test");
    window.setFramerateLimit(60);

    // ImGui-SFML 2.6 초기화 - 더 간단한 방식
    ImGui::SFML::Init(window);

    std::cout << "ImGui-SFML initialized successfully!" << std::endl;

    // 테스트용 변수들
    bool show_demo_window = true;
    bool show_test_window = true;
    float test_float = 0.0f;
    int test_counter = 0;
    sf::Color clear_color = sf::Color::Black;

    // SFML 2.6.1 스타일 그래픽 요소
    sf::CircleShape shape(50.f);
    shape.setFillColor(sf::Color::Green);
    shape.setPosition(100.f, 100.f);

    sf::Clock deltaClock;

    while (window.isOpen())
    {
        sf::Event event;
        while (window.pollEvent(event))
        {
            ImGui::SFML::ProcessEvent(event);

            if (event.type == sf::Event::Closed)
            {
                window.close();
            }
        }

        // ImGui-SFML 2.6 프레임 시작
        ImGui::SFML::Update(window, deltaClock.restart());

        // 1. ImGui 1.89.9 데모 윈도우
        if (show_demo_window)
        {
            ImGui::ShowDemoWindow(&show_demo_window);
        }

        // 2. 커스텀 테스트 윈도우 (ImGui 1.89.9 호환)
        if (show_test_window)
        {
            ImGui::Begin("ImGui-SFML Test Window", &show_test_window);

            // 기본 텍스트
            ImGui::Text("ImGui-SFML is working!");
            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
                        1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

            ImGui::Separator();

            // 버튼 테스트
            if (ImGui::Button("Test Button"))
            {
                test_counter++;
                std::cout << "Button clicked! Count: " << test_counter << std::endl;
            }
            ImGui::SameLine();
            ImGui::Text("counter = %d", test_counter);

            // 슬라이더 테스트 (ImGui 1.89.9 스타일)
            ImGui::SliderFloat("Test Float", &test_float, 0.0f, 1.0f);

            // 컬러 에디터 테스트 (ImGui 1.89.9 호환)
            static float color[4] = {0.0f, 0.0f, 0.0f, 1.0f};
            if (ImGui::ColorEdit3("Clear Color", color))
            {
                clear_color = sf::Color(
                    static_cast<sf::Uint8>(color[0] * 255),
                    static_cast<sf::Uint8>(color[1] * 255),
                    static_cast<sf::Uint8>(color[2] * 255)
                );
            }

            // 체크박스 테스트
            ImGui::Checkbox("Show Demo Window", &show_demo_window);

            ImGui::Separator();

            // SFML 2.6.1과의 상호작용 테스트
            ImGui::Text("SFML Integration Test:");
            if (ImGui::Button("Move Circle"))
            {
                shape.setPosition(
                    static_cast<float>(rand() % 700),
                    static_cast<float>(rand() % 500)
                );
            }

            sf::Vector2f pos = shape.getPosition();
            ImGui::Text("Circle Position: (%.1f, %.1f)", pos.x, pos.y);

            // ImGui 1.89.9 호환 입력 테스트
            static char text_buffer[256] = "Hello, World!";
            ImGui::InputText("Text Input", text_buffer, sizeof(text_buffer));

            ImGui::End();
        }

        // 3. ImGui 1.89.9 스타일 메뉴바
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("Exit"))
                {
                    window.close();
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Windows"))
            {
                ImGui::MenuItem("Demo Window", nullptr, &show_demo_window);
                ImGui::MenuItem("Test Window", nullptr, &show_test_window);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Help"))
            {
                if (ImGui::MenuItem("About"))
                {
                    std::cout << "ImGui " << IMGUI_VERSION
                        << " + SFML " << SFML_VERSION_MAJOR << "." << SFML_VERSION_MINOR
                        << " + ImGui-SFML 2.6" << std::endl;
                }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        // SFML 2.6.1 렌더링
        window.clear(clear_color);
        window.draw(shape);

        // ImGui-SFML 2.6 렌더링
        ImGui::SFML::Render(window);

        window.display();
    }

    // ImGui-SFML 2.6 정리
    ImGui::SFML::Shutdown();
    core::AppContext::getInstance().cleanup();

    std::cout << "ImGui-SFML test completed successfully!" << std::endl;
    return 0;
}
