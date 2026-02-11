#include "engine_log.h"
#include <unordered_set>
#include <cstdio>
#include <dxgidebug.h>
#include <Windows.h>
#include <dxgidebug.h>
#include <dxgi1_3.h>
#include <d3d11.h>

#pragma comment(lib, "dxguid.lib")

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

void ReportDXGILiveObjects()
{
    IDXGIDebug* pDebug = nullptr;

    // Create DXGI debug interface
    if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&pDebug))))
    {
        std::cout << "Reporting live DXGI objects...\n";
        pDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_SUMMARY);
        pDebug->Release();
    }
    else
    {
        std::cerr << "Failed to get DXGI debug interface.\n";
    }
}