#pragma once
#pragma once
#include <string>
#include <vector>
#include <sol/sol.hpp>

// Lua 바인딩용
void BindLuaLogging(sol::state& lua);

// 프레임 로그 처리
void LogPush(const std::string& msg);
void LogPushOnce(const std::string& msg);
void FlushLogs();

// Lua 에러 기록
void LogLuaError(const std::string& func, const std::string& msg);
void ResetLuaLogState();
