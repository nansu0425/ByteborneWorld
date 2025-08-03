#include "KoreanInputManager.h"
#include <spdlog/spdlog.h>

#ifdef _WIN32
#pragma comment(lib, "imm32.lib")
#endif

KoreanInputManager::KoreanInputManager()
    : m_koreanInputEnabled(false)
    , m_lastIMEState(false)
    , m_windowHandle(nullptr)
{
}

void KoreanInputManager::initialize(void* windowHandle)
{
    m_windowHandle = windowHandle;
    
#ifdef _WIN32
    m_koreanInputEnabled = true;
    spdlog::info("[KoreanInputManager] Korean input support enabled on Windows");
#else
    m_koreanInputEnabled = true;
    spdlog::info("[KoreanInputManager] Korean input support enabled");
#endif
}

bool KoreanInputManager::isKoreanInputActive() const
{
#ifdef _WIN32
    if (!m_windowHandle)
    {
        return m_lastIMEState;
    }

    if (!isKoreanLayout())
    {
        m_lastIMEState = false;
        return false;
    }

    return checkIMEStatus();
#else
    return m_koreanInputEnabled;
#endif
}

void KoreanInputManager::updateInputState()
{
#ifdef _WIN32
    static bool lastKoreanState = false;
    static bool isInitialized = false;
    
    bool currentKoreanState = isKoreanInputActive();
    
    // 첫 번째 호출이거나 상태가 변경된 경우
    if (!isInitialized || lastKoreanState != currentKoreanState)
    {
        lastKoreanState = currentKoreanState;
        isInitialized = true;
        
        if (currentKoreanState)
        {
            spdlog::debug("[KoreanInputManager] Korean input mode activated (한글 입력)");
        }
        else
        {
            spdlog::debug("[KoreanInputManager] English input mode activated (영어 입력)");
        }
    }
#endif
}

#ifdef _WIN32
bool KoreanInputManager::isKoreanLayout() const
{
    HWND hwnd = static_cast<HWND>(m_windowHandle);
    DWORD threadId = GetWindowThreadProcessId(hwnd, nullptr);
    HKL hkl = GetKeyboardLayout(threadId);
    LANGID langId = LOWORD(hkl);
    
    return (PRIMARYLANGID(langId) == LANG_KOREAN);
}

bool KoreanInputManager::checkIMEStatus() const
{
    HWND hwnd = static_cast<HWND>(m_windowHandle);
    HIMC hIMC = ImmGetContext(hwnd);
    
    if (hIMC)
    {
        // IME가 열려있는지 확인
        BOOL isOpen = ImmGetOpenStatus(hIMC);
        
        // 추가로 IME 변환 모드도 확인
        DWORD convMode = 0;
        DWORD sentMode = 0;
        if (ImmGetConversionStatus(hIMC, &convMode, &sentMode))
        {
            // 한글 모드인지 확인 (IME_CMODE_NATIVE 플래그)
            bool isNativeMode = (convMode & IME_CMODE_NATIVE) != 0;
            ImmReleaseContext(hwnd, hIMC);
            
            bool result = isOpen && isNativeMode;
            m_lastIMEState = result;
            return result;
        }
        
        ImmReleaseContext(hwnd, hIMC);
        
        // 변환 상태를 가져올 수 없으면 열린 상태만으로 판단
        bool result = isOpen != FALSE;
        m_lastIMEState = result;
        return result;
    }
    
    // IME 컨텍스트를 얻을 수 없으면 마지막 상태 유지
    return m_lastIMEState;
}
#endif
