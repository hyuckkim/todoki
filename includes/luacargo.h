#pragma once
#include "includesol.h"
#include <string>
#include <optional>
#include <functional>

struct LuaBinding {
    std::function<void(sol::state&, const char*)> bindFunc;
    std::string name;
};

class LuaCargo
{ 
public:
	bool Init(const char* entry, const std::vector<LuaBinding>& systems = {});

	template<typename T = void, typename... Args>
	T Call(const char* func, Args&&... args) {
        sol::protected_function f = lua[func];

        // 리턴 타입이 void인 경우
        if constexpr (std::is_void_v<T>) {
            if (f.valid()) {
                auto result = f(std::forward<Args>(args)...);
                HandleResult(func, result);
            }
        }
        // 리턴 타입이 있는 경우 (Nullable)
        else {
            if (!f.valid()) return std::optional<T>(std::nullopt);

            auto result = f(std::forward<Args>(args)...);
            if (result.valid()) {
                return std::optional<T>(result.get<T>());
            }
            else {
                HandleResult(func, result);
                return std::optional<T>(std::nullopt);
            }
        }
	}

private:
	sol::state lua;
    void HandleResult(
        const std::string& func,
        const sol::protected_function_result& result
    );
};