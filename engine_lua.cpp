#include "sol.h"
#include "engine_lua.h"
#include "engine_log.h"

sol::state lua;
static std::string g_last_lua_error = "";

void InitLuaEngine(const char* entry) {

    lua = sol::state();
    lua.collect_garbage();

    lua.open_libraries(
        sol::lib::base,
        sol::lib::package,
        sol::lib::table,
        sol::lib::string,
        sol::lib::math,
        sol::lib::debug,
        sol::lib::utf8,
        sol::lib::coroutine,
        sol::lib::os
    );


    auto load_result = lua.script_file(entry, sol::script_pass_on_error);
    if (!load_result.valid()) {
        sol::error err = load_result;
        printf("[LUA ERROR] %s\n", err.what());
        return;
    }
    printf("Lua Engine Initialized / Reloaded via sol2.\n");
}
void RegisterLuaLibs() {
    BindLuaLogging(lua);
    register_sys(lua, "sys");
    register_input(lua, "is");
    register_draw(lua, "g");
    register_res(lua, "res");
}

LuaConfig LoadLuaConfig() {
    return LuaConfig{
        lua.get_or("ScreenWidth", 800),
        lua.get_or("ScreenHeight", 600),
        lua.get_or<std::string>("WindowTitle", "Untitled Project")
    };
}

void HandleLuaResult(const std::string& func_name, const sol::protected_function_result& result)
{
    if (!result.valid()) {
        try {
            sol::error err = result;
            std::string current_error = err.what();

            if (g_last_lua_error != current_error) {
                printf("[LUA ERROR] %s: %s\n", func_name.c_str(), current_error.c_str());
                g_last_lua_error = current_error;
            }
        }
        catch (const std::exception& e) {
            printf("[CRITICAL ERROR] Failed to parse Lua error: %s\n", e.what());
        }
        catch (...) {
            printf("[CRITICAL ERROR] Unknown exception during Lua error handling\n");
        }
    }
}
