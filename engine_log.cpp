#include "engine_log.h"
#include <unordered_set>
#include <cstdio>

static std::vector<std::string> g_frameLogBuffer;
static std::unordered_set<std::string> g_printedMessages;
static std::string g_last_lua_error;

void ResetLuaLogState() {
    g_frameLogBuffer.clear();
    g_printedMessages.clear();
    g_last_lua_error.clear();
}

void LogPush(const std::string& msg) {
    g_frameLogBuffer.push_back(msg);
}

void LogPushOnce(const std::string& msg) {
    if (g_printedMessages.insert(msg).second) {
        g_frameLogBuffer.push_back(msg);
    }
}

void FlushLogs() {
    for (const auto& log : g_frameLogBuffer) {
        printf("%s\n", log.c_str());
    }
    g_frameLogBuffer.clear();
}

void LogLuaError(const std::string& func, const std::string& msg) {
    if (g_last_lua_error != msg) {
        printf("[LUA ERROR] %s: %s\n", func.c_str(), msg.c_str());
        g_last_lua_error = msg;
    }
}
