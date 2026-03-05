#include "colorparser.h"

#include <cassert>
#include <cmath>

namespace {
    constexpr float kEpsilon = 0.0001f;

    bool NearlyEqual(float lhs, float rhs) {
        return std::fabs(lhs - rhs) < kEpsilon;
    }

    void AssertColor(const D2D1::ColorF& c, float r, float g, float b, float a) {
        assert(NearlyEqual(c.r, r));
        assert(NearlyEqual(c.g, g));
        assert(NearlyEqual(c.b, b));
        assert(NearlyEqual(c.a, a));
    }
}

int main() {
    {
        auto c = ColorParser::FromHex(0xFF8040, 1.0f);
        AssertColor(c, 1.0f, 128.0f / 255.0f, 64.0f / 255.0f, 1.0f);
    }

    {
        auto c = ColorParser::FromHex(0x112233, 128.0f);
        AssertColor(c, 17.0f / 255.0f, 34.0f / 255.0f, 51.0f / 255.0f, 128.0f / 255.0f);
    }

    {
        auto c = ColorParser::FromRGBA(255.0f, 128.0f, 64.0f, 128.0f);
        AssertColor(c, 1.0f, 128.0f / 255.0f, 64.0f / 255.0f, 128.0f / 255.0f);
    }

    {
        auto c = ColorParser::FromRGBA(0.1f, 0.2f, 0.3f, 0.4f);
        AssertColor(c, 0.1f, 0.2f, 0.3f, 0.4f);
    }

    {
        auto c = ColorParser::FromString("#336699");
        AssertColor(c, 51.0f / 255.0f, 102.0f / 255.0f, 153.0f / 255.0f, 1.0f);
    }

    {
        auto c = ColorParser::FromString("336699", 0.5f);
        AssertColor(c, 51.0f / 255.0f, 102.0f / 255.0f, 153.0f / 255.0f, 0.5f);
    }

    {
        auto c = ColorParser::FromString("33669980");
        AssertColor(c, 51.0f / 255.0f, 102.0f / 255.0f, 153.0f / 255.0f, 128.0f / 255.0f);
    }

    {
        auto c = ColorParser::FromString("12345");
        AssertColor(c, 0.0f, 0.0f, 0.0f, 1.0f);
    }

    return 0;
}
