#pragma once
#include "sol.h"

extern sol::state lua;

void InitLuaEngine(const char* entry);
void RebuildAllBitmaps();

void register_draw(sol::state& lua, const char* name);
void register_input(sol::state& lua, const char* name);
void register_sys(sol::state& lua, const char* name);
void register_res(sol::state& lua, const char* name);

void HandleLuaResult(const std::string& func_name, const sol::protected_function_result& result);

template<typename... Args>
void Call(const std::string& func_name, Args&&... args) {
    sol::protected_function f = lua[func_name];
    if (!f.valid()) return;
    auto result = f(std::forward<Args>(args)...);
    HandleLuaResult(func_name, result);
}

struct LuaConfig {
    int width;
    int height;
    std::string title;
};
LuaConfig LoadLuaConfig();