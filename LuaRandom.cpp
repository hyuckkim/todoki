#include <sol/sol.hpp>
#include <cstdint>

struct FastRNG {
    uint32_t state;
    FastRNG(uint32_t seed) : state(seed == 0 ? 0xACE1u : seed) {}

    uint32_t next() {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }

    int nextRange(int min, int max) {
        if (min >= max) return min;
        return min + (static_cast<int>(next() % (max - min + 1)));
    }

    float nextFloat() {
        return static_cast<float>(next()) / static_cast<float>(0xFFFFFFFFu);
    }
};

// 2. 바인딩 함수
void register_random_module(sol::state_view& lua, const char* name) {
    // 먼저 Random이라는 타입을 루아에 알려줍니다 (메서드 사용을 위해)
    lua.new_usertype<FastRNG>("RNGGenerator",
        "next", &FastRNG::next,
        "range", &FastRNG::nextRange,
        "float", &FastRNG::nextFloat
    );
    sol::table res = lua[name].get_or_create<sol::table>();

    // res.random(seed) 호출 시 새로운 RNGGenerator 객체를 반환하는 함수 등록
    res["random"] = [](uint32_t seed) -> FastRNG {
        return FastRNG(seed);
        };
}