#include "includesol.h"
#include "luacargo.h"
#include "luadef.h"

bool LuaCargo::Init(const char* entry, const std::vector<LuaBinding>& systems) {
	lua = sol::state();

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

#ifdef _DEBUG
    LuaDefBuilder defBuilder;
    LuaDefBuilder* defPtr = &defBuilder;
#else
    LuaDefBuilder* defPtr = nullptr;
#endif

    for (const auto& system : systems) {
        if (system.bindFunc) {
            LuaBindContext ctx{ lua, defPtr, system.name.c_str() };
            system.bindFunc(ctx);
            printf("[LUA] System '%s' bound successfully.\n", system.name.c_str());
        }
    }

#ifdef _DEBUG
    defBuilder.Write("_globalDef.lua");
#endif

    auto load_result = lua.script_file(entry, sol::script_pass_on_error);
    if (!load_result.valid()) {
        sol::error err = load_result;
        printf("[LUA ERROR] %s\n", err.what());
        return false;
    }
    printf("Lua Engine Initialized.\n");
    return true;
}

void LuaCargo::HandleResult(const std::string& func, const sol::protected_function_result& result)
{
    if (!result.valid()) {
        try {
            sol::error err = result;
            std::string current_error = err.what();

            printf("[LUA ERROR] %s: %s\n", func.c_str(), current_error.c_str());
        }
        catch (const std::exception& e) {
            printf("[CRITICAL ERROR] Failed to parse Lua error: %s\n", e.what());
        }
        catch (...) {
            printf("[CRITICAL ERROR] Unknown exception during Lua error handling\n");
        }
    }
}