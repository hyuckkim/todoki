#include "engine_sound.h"
#include "sol.h"
#include "packManager.cpp"

void register_sound(sol::state& lua, const char* name) {
    auto s = lua.create_named_table(name);

    s["play"] = [](int id, sol::optional<bool> loop, sol::optional<float> volume) -> unsigned int {
        if (id < 0 || id >= g_soundTable.size()) return 0u;
        auto h = EngineSound::Instance().Play(g_soundTable[id]);
        };

    s["stop"] = [](unsigned int handle) {
        EngineSound::Instance().Stop(handle);
        };

    s["volume"] = [](unsigned int handle, float vol) {
        EngineSound::Instance().SetVolume(handle, vol);
        };
}