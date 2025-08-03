#pragma once

#include "imgui.h"
#include <string>
#include <vector>

class FontManager
{
public:
    FontManager();
    ~FontManager() = default;

    // 복사/이동 방지
    FontManager(const FontManager&) = delete;
    FontManager& operator=(const FontManager&) = delete;
    FontManager(FontManager&&) = delete;
    FontManager& operator=(FontManager&&) = delete;

    bool initializeKoreanFont();
    
    ImFont* getKoreanFont() const { return m_koreanFont; }
    ImFont* getDefaultFont() const { return m_defaultFont; }
    
    void pushKoreanFont() const;
    void popKoreanFont() const;

private:
    ImFont* m_koreanFont;
    ImFont* m_defaultFont;
    bool m_fontLoaded;
    
    static const ImWchar* getKoreanRanges();
    std::vector<std::string> getSystemFontPaths() const;
};
