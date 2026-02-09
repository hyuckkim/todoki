#include "sol.h"
#include "engine_log.h"

void BindLuaLogging(sol::state& lua) {
    lua["print"] = [](sol::variadic_args args, sol::this_state s) {
        sol::state_view L(s);
        sol::function tostring = L["tostring"];

        std::string msg;
        for (auto v : args) {
            msg += tostring(v.get<sol::object>()).get<std::string>();
            msg += "  ";
        }
        LogPush(msg);
        };

    lua["printOnce"] = [](sol::variadic_args args, sol::this_state s) {
        sol::state_view L(s);
        sol::function tostring = L["tostring"];

        std::string msg;
        for (auto v : args) {
            msg += tostring(v.get<sol::object>()).get<std::string>();
            msg += "  ";
        }
        LogPushOnce(msg);
        };
}
