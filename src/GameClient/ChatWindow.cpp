#include "ChatWindow.h"
#include "imgui.h"

ChatWindow::ChatWindow(FontManager& fontManager, KoreanInputManager& inputManager)
    : m_fontManager(fontManager)
    , m_inputManager(inputManager)
    , m_isVisible(true)
    , m_autoScroll(true)
    , m_scrollToBottom(false)
    , m_usernameText("플레이어")
{
    // 환영 메시지 추가
    addChatMessage("시스템", "채팅창에 오신 것을 환영합니다!");
    addChatMessage("시스템", "한글 입력이 완벽하게 지원됩니다.");
    addChatMessage("시스템", "메시지를 입력하고 Enter 키를 눌러보세요.");
}

void ChatWindow::render()
{
    if (!m_isVisible) return;

    m_fontManager.pushKoreanFont();

    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("채팅창 - 한글 완벽 지원", &m_isVisible))
    {
        renderToolbar();
        ImGui::Separator();
        renderMessageArea();
        ImGui::Separator();
        renderInputArea();
    }
    ImGui::End();

    m_fontManager.popKoreanFont();
}

void ChatWindow::renderToolbar()
{
    ImGui::Text("사용자명:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120);

    // 사용자명 입력
    char usernameBuffer[256];
    strncpy_s(usernameBuffer, m_usernameText.c_str(), sizeof(usernameBuffer) - 1);
    usernameBuffer[sizeof(usernameBuffer) - 1] = '\0';

    if (ImGui::InputText("##username", usernameBuffer, sizeof(usernameBuffer)))
    {
        m_usernameText = std::string(usernameBuffer);
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
        addTestMessages();
    }
}

void ChatWindow::renderMessageArea()
{
    const float footerHeightToReserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
    if (ImGui::BeginChild("ChatMessages", ImVec2(0, -footerHeightToReserve), false, ImGuiWindowFlags_HorizontalScrollbar))
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
}

void ChatWindow::renderInputArea()
{
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
    char inputBuffer[1024];
    strncpy_s(inputBuffer, m_chatInputText.c_str(), sizeof(inputBuffer) - 1);
    inputBuffer[sizeof(inputBuffer) - 1] = '\0';

    bool enterPressed = ImGui::InputText("##chatinput", inputBuffer, sizeof(inputBuffer),
                                          ImGuiInputTextFlags_EnterReturnsTrue);

    // 입력 내용 업데이트
    m_chatInputText = std::string(inputBuffer);

    ImGui::SameLine();
    bool sendButton = ImGui::Button("전송");

    // 한글 입력 상태 표시
    renderInputStatusIndicator();

    // Enter 키 또는 전송 버튼 클릭 시 메시지 전송
    if (enterPressed || sendButton)
    {
        sendChatMessage();
        setFocusOnInput = true; // 다음 프레임에서 포커스 재설정
    }
}

void ChatWindow::renderInputStatusIndicator()
{
    ImGui::SameLine();
    bool koreanActive = m_inputManager.isKoreanInputActive();
    if (koreanActive)
    {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "[한]");
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("한글 입력 모드 (Alt+한/영으로 전환)");
        }
    }
    else
    {
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "[영]");
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("영어 입력 모드 (Alt+한/영으로 전환)");
        }
    }
}

void ChatWindow::sendChatMessage()
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

void ChatWindow::addChatMessage(const std::string& sender, const std::string& message)
{
    ChatMessage chatMsg;
    chatMsg.sender = sender;
    chatMsg.message = message;
    chatMsg.timestamp = getCurrentTimestamp();
    
    m_chatMessages.push_back(chatMsg);
    
    // 최대 1000개 메시지만 보관 (메모리 절약)
    if (m_chatMessages.size() > 1000)
    {
        m_chatMessages.erase(m_chatMessages.begin());
    }
    
    // 메시지 추가 로그 (UTF-8 지원)
    spdlog::info("[Chat] {}: {}", sender, message);
}

void ChatWindow::scrollChatToBottom()
{
    m_scrollToBottom = true;
}

void ChatWindow::addTestMessages()
{
    addChatMessage("테스트", "안녕하세요! 한글이 잘 보이나요? 🇰🇷");
    addChatMessage("테스트", "가나다라마바사아자차카타파하");
    addChatMessage("테스트", "ㄱㄴㄷㄹㅁㅂㅅㅇㅈㅊㅋㅌㅍㅎ");
    addChatMessage("테스트", "ㅏㅑㅓㅕㅗㅛㅜㅠㅡㅣ");
}

std::string ChatWindow::getCurrentTimestamp() const
{
    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%H:%M:%S");
    return oss.str();
}
