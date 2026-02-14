#pragma once
#include "sol.h"
#include <optional>

extern sol::state lua;

void InitLuaEngine(const char* entry);
void RegisterLuaLibs();
void RebuildAllBitmaps();

void register_draw(sol::state& lua, const char* name);
void register_input(sol::state& lua, const char* name);
void register_sys(sol::state& lua, const char* name);
void register_res(sol::state& lua, const char* name);
void register_sound(sol::state& lua, const char* name);

void unregisterLuaFunctions();

void HandleLuaResult(const std::string& func_name, const sol::protected_function_result& result);

template<typename T = void, typename... Args>
auto Call(const std::string& func_name, Args&&... args) {
    sol::protected_function f = lua[func_name];

    // 리턴 타입이 void인 경우
    if constexpr (std::is_void_v<T>) {
        if (f.valid()) {
            auto result = f(std::forward<Args>(args)...);
            HandleLuaResult(func_name, result);
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
            HandleLuaResult(func_name, result);
            return std::optional<T>(std::nullopt);
        }
    }
}

struct LuaConfig {
    int width;
    int height;
    std::string title;
};
LuaConfig LoadLuaConfig();