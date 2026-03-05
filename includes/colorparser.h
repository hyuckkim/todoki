#include <cstdint>
#include <optional>
#include <string>

#include <d2d1.h>

class ColorParser {
public:
    // A. 16진수 정수 + Alpha (0xRRGGBB, alpha)
    static D2D1::ColorF FromHex(uint32_t hex, float alpha = 1.0f) {
        float a = (alpha > 1.0f) ? alpha / 255.f : alpha;
        return D2D1::ColorF(hex, a);
    }

    // B. RGBA 개별 값 (0~255 또는 0.0~1.0 혼용 대응)
    static D2D1::ColorF FromRGBA(float r, float g, float b, float a = 1.0f) {
        bool isIntRange = (r > 1.0f || g > 1.0f || b > 1.0f || a > 1.0f);
        if (isIntRange) {
            return D2D1::ColorF(r / 255.f, g / 255.f, b / 255.f, a / 255.f);
        }
        return D2D1::ColorF(r, g, b, a);
    }

    // C. 문자열 처리 (#RRGGBB, #RRGGBBAA)
    static D2D1::ColorF FromString(std::string s, std::optional<float> fallbackAlpha = std::nullopt) {
        if (!s.empty() && s[0] == '#') s = s.substr(1);
        if (s.length() != 6 && s.length() != 8) return D2D1::ColorF(D2D1::ColorF::Black);

        uint32_t hex = std::stoul(s, nullptr, 16);

        if (s.length() == 8) { // RRGGBBAA
            return D2D1::ColorF(
                ((hex >> 24) & 0xFF) / 255.f,
                ((hex >> 16) & 0xFF) / 255.f,
                ((hex >> 8) & 0xFF) / 255.f,
                (hex & 0xFF) / 255.f
            );
        }
        // RRGGBB + optional alpha
        return FromHex(hex, fallbackAlpha.value_or(1.0f));
    }
};