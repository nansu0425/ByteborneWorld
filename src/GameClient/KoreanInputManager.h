#pragma once

#ifdef _WIN32
#include <windows.h>
#include <imm.h>
#endif

class KoreanInputManager
{
public:
    KoreanInputManager();
    ~KoreanInputManager() = default;

    // 복사/이동 방지
    KoreanInputManager(const KoreanInputManager&) = delete;
    KoreanInputManager& operator=(const KoreanInputManager&) = delete;
    KoreanInputManager(KoreanInputManager&&) = delete;
    KoreanInputManager& operator=(KoreanInputManager&&) = delete;

    void initialize(void* windowHandle);
    bool isKoreanInputActive() const;
    void updateInputState();

private:
    bool m_koreanInputEnabled;
    mutable bool m_lastIMEState;
    void* m_windowHandle;

#ifdef _WIN32
    bool checkIMEStatus() const;
    bool isKoreanLayout() const;
#endif
};
