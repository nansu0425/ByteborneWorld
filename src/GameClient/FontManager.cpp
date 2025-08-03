#include "FontManager.h"
#include "imgui-SFML.h"

FontManager::FontManager()
    : m_koreanFont(nullptr)
    , m_defaultFont(nullptr)
    , m_fontLoaded(false)
{
}

bool FontManager::initializeKoreanFont()
{
    ImGuiIO& io = ImGui::GetIO();
    
    // 기본 폰트 먼저 로드
    m_defaultFont = io.Fonts->AddFontDefault();
    
    const ImWchar* korean_ranges = getKoreanRanges();
    auto font_paths = getSystemFontPaths();
    
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
                m_fontLoaded = true;
                spdlog::info("[FontManager] Korean font loaded: {}", font_path);
                
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
    
    if (!m_fontLoaded)
    {
        spdlog::warn("[FontManager] Could not load Korean font, using default font with Korean ranges");
        
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
    
    spdlog::info("[FontManager] Font atlas built with Korean character support");
    return m_fontLoaded;
}

void FontManager::pushKoreanFont() const
{
    if (m_koreanFont)
    {
        ImGui::PushFont(m_koreanFont);
    }
}

void FontManager::popKoreanFont() const
{
    if (m_koreanFont)
    {
        ImGui::PopFont();
    }
}

const ImWchar* FontManager::getKoreanRanges()
{
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
    return korean_ranges;
}

std::vector<std::string> FontManager::getSystemFontPaths() const
{
    return {
        "C:/Windows/Fonts/malgun.ttf",     // 맑은 고딕
        "C:/Windows/Fonts/malgunbd.ttf",   // 맑은 고딕 Bold
        "C:/Windows/Fonts/NanumGothic.ttf", // 나눔고딕
        "C:/Windows/Fonts/gulim.ttc",      // 굴림
        "C:/Windows/Fonts/batang.ttc",     // 바탕
        "C:/Windows/Fonts/dotum.ttc",      // 돋움
        "C:/Windows/Fonts/gungsuh.ttc",    // 궁서
    };
}
