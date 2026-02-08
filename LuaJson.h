#pragma once
#include "lua_engine.h"

// JSON 비동기 로딩을 위한 태스크
struct JsonTask : public ITask {
    std::string path;
    std::future<nlohmann::json> fuel;
    sol::object result = sol::nil;

    bool check(sol::this_state s) override;
    sol::object getResult() override;
};

// 외부(lua_engine.cpp 등)에서 호출할 등록 함수
void register_json_module(sol::state_view& lua, const char* namespace_name = "res");

// 헬퍼 함수: JSON 노드를 루아 객체(숫자, 문자열, 혹은 JsonNode)로 변환
sol::object wrap_json_node(nlohmann::json& j, sol::state_view lua);