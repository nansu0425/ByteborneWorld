#include "ChatWindow.h"
#include "imgui.h"
#include <spdlog/spdlog.h>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <algorithm>
#include <chrono>

ChatWindow::ChatWindow(FontManager& fontManager, KoreanInputManager& inputManager)
    : m_fontManager(fontManager)
    , m_inputManager(inputManager)
    , m_isVisible(true)
    , m_autoScroll(true)
    , m_scrollToBottom(false)
    , m_connected(false)  // 연결 상태 초기화 추가
    , m_usernameText("플레이어")
{
    // 환영 메시지 추가
    addChatMessage("시스템", "채팅창에 오신 것을 환영합니다!");
    addChatMessage("시스템", "한글 입력이 완벽하게 지원됩니다.");
    addChatMessage("시스템", "서버에 연결 중입니다...");
}

void ChatWindow::render()
{
    if (!m_isVisible) return;

    m_fontManager.pushKoreanFont();

    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("채팅창 - 네트워크 통신 지원", &m_isVisible))
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
    renderConnectionStatus();  // 연결 상태 표시

    ImGui::SameLine();
    ImGui::Checkbox("자동 스크롤", &m_autoScroll);

    ImGui::SameLine();
    if (ImGui::Button("채팅 지우기"))
    {
        m_chatMessages.clear();
        m_pendingIndex.clear();
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

            // 발신자 이름 표시 (색상으로 구분)
            ImVec4 senderColor = ImVec4(1.0f, 1.0f, 0.0f, 1.0f); // 기본 노란색
            if (msg.sender == "서버" || msg.sender == "Server")
            {
                senderColor = ImVec4(0.0f, 1.0f, 0.0f, 1.0f); // 서버 메시지는 초록색
            }
            else if (msg.sender == "시스템" || msg.sender == "System")
            {
                senderColor = ImVec4(0.8f, 0.8f, 0.8f, 1.0f); // 시스템 메시지는 회색
            }
            
            // pending이면 반투명 처리
            if (msg.pending)
            {
                senderColor.w = 0.6f;
            }
            
            ImGui::TextColored(senderColor, "%s:", msg.sender.c_str());
            ImGui::SameLine();

            // 메시지 내용 표시 (흰색)
            ImVec4 textColor = ImVec4(1,1,1,1);
            if (msg.pending) textColor.w = 0.7f;
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

void ChatWindow::renderConnectionStatus()
{
    if (m_connected)
    {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "[연결됨]");
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("서버에 연결되었습니다");
        }
    }
    else
    {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "[연결 끊김]");
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("서버에 연결되지 않았습니다");
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
    
    // 연결 상태 확인
    if (!m_connected)
    {
        addChatMessage("시스템", "서버에 연결되지 않았습니다. 메시지를 전송할 수 없습니다.");
        m_chatInputText.clear();
        scrollChatToBottom();
        return;
    }
    
    // 네트워크를 통해 메시지 전송 (콜백이 설정된 경우)
    if (m_sendMessageCallback)
    {
        m_sendMessageCallback(m_chatInputText);
    }
    
    // 낙관적 UI 메시지는 GameClient에서 client_message_id를 생성한 뒤 추가한다.
    // 여기서는 입력만 초기화하고 스크롤만 처리.
    m_chatInputText.clear();
    scrollChatToBottom();
}

void ChatWindow::addChatMessage(const std::string& sender, const std::string& message)
{
    ChatMessage chatMsg;
    chatMsg.sender = sender;
    chatMsg.message = message;
    chatMsg.timestamp = getCurrentTimestamp();
    
    // 로컬/시스템 메시지에도 정렬을 위해 클라 기준 ms를 기록
    chatMsg.serverSentAtMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    m_chatMessages.push_back(chatMsg);

    // 최신이 아래로 가도록 정렬 유지
    sortMessagesByServerOrder();
    scrollChatToBottom();
    
    // 최대 1000개 메시지만 보관 (메모리 절약)
    if (m_chatMessages.size() > 1000)
    {
        m_chatMessages.erase(m_chatMessages.begin());
    }
    
    // 메시지 추가 로그 (UTF-8 지원)
    spdlog::info("[Chat] {}: {}", sender, message);
}

void ChatWindow::addLocalPendingMessage(const std::string& sender, const std::string& message, uint64_t clientMessageId)
{
    ChatMessage chatMsg;
    chatMsg.sender = sender;
    chatMsg.message = message;
    chatMsg.pending = true;
    chatMsg.clientMessageId = clientMessageId;
    chatMsg.timestamp = getCurrentTimestamp();

    m_pendingIndex[clientMessageId] = m_chatMessages.size();
    m_chatMessages.push_back(std::move(chatMsg));
    scrollChatToBottom();
}

bool ChatWindow::ackPendingMessage(uint64_t clientMessageId,
                                   const std::string& authoritativeSender,
                                   const std::string& authoritativeContent,
                                   uint64_t serverMessageId,
                                   int64_t serverSentAtMs)
{
    auto it = m_pendingIndex.find(clientMessageId);
    if (it == m_pendingIndex.end())
    {
        return false; // 매칭 실패
    }

    auto& msg = m_chatMessages[it->second];
    msg.sender = authoritativeSender;
    msg.message = authoritativeContent;
    msg.pending = false;
    msg.serverMessageId = serverMessageId;
    msg.serverSentAtMs = serverSentAtMs;
    msg.timestamp = formatTimestampFromMs(serverSentAtMs);

    m_pendingIndex.erase(it);
    sortMessagesByServerOrder();
    scrollChatToBottom();
    return true;
}

void ChatWindow::addAuthoritativeMessage(const std::string& sender,
                                         const std::string& content,
                                         uint64_t serverMessageId,
                                         int64_t serverSentAtMs,
                                         uint64_t clientMessageId)
{
    ChatMessage chatMsg;
    chatMsg.sender = sender;
    chatMsg.message = content;
    chatMsg.pending = false;
    chatMsg.clientMessageId = clientMessageId;
    chatMsg.serverMessageId = serverMessageId;
    chatMsg.serverSentAtMs = serverSentAtMs;
    chatMsg.timestamp = formatTimestampFromMs(serverSentAtMs);

    m_chatMessages.push_back(std::move(chatMsg));
    sortMessagesByServerOrder();
    scrollChatToBottom();
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

std::string ChatWindow::formatTimestampFromMs(int64_t msSinceEpoch)
{
    std::time_t tt = static_cast<std::time_t>(msSinceEpoch / 1000);
    std::tm tm = *std::localtime(&tt);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%H:%M:%S");
    return oss.str();
}

void ChatWindow::sortMessagesByServerOrder()
{
    // 최신 메시지가 아래로 가도록(오래된 것이 먼저, 새로운 것이 나중)
    std::stable_sort(m_chatMessages.begin(), m_chatMessages.end(), [](const ChatMessage& a, const ChatMessage& b){
        // pending은 항상 마지막에 위치
        if (a.pending != b.pending)
            return b.pending; // a가 pending=false 이면 true 반환 → a 먼저

        // 권위/로컬 모두 serverSentAtMs 기준 정렬(오름차순)
        if (a.serverSentAtMs != b.serverSentAtMs)
            return a.serverSentAtMs < b.serverSentAtMs;
        
        // 동시간대에는 serverMessageId로 안정적 정렬(0은 자연히 뒤로 감)
        if (a.serverMessageId != b.serverMessageId)
            return a.serverMessageId < b.serverMessageId;

        // 마지막으로 clientMessageId로 타이 브레이크
        return a.clientMessageId < b.clientMessageId;
    });
}
