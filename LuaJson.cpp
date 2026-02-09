#include "luajson.h"
#include <fstream>

using json = nlohmann::json;

// 파일 스코프 정적 변수로 캡슐화
static std::unordered_map<std::string, std::unique_ptr<json>> g_JsonCache;
static std::mutex g_JsonMutex;

sol::object wrap_json_node(json& j, sol::state_view lua) {
    if (j.is_structured()) {
        return sol::make_object(lua, JsonNode{ &j });
    }
    if (j.is_string())  return sol::make_object(lua, j.get<std::string>());
    if (j.is_number())  return sol::make_object(lua, j.get<double>());
    if (j.is_boolean()) return sol::make_object(lua, j.get<bool>());
    return sol::nil;
}

// --- JsonTask 구현 ---
bool JsonTask::check(sol::this_state s) {
    if (isDone) return true;

    if (fuel.valid() && fuel.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        std::lock_guard<std::mutex> lock(g_JsonMutex);

        try {
            g_JsonCache[path] = std::make_unique<json>(fuel.get());
            sol::state_view lua(s);
            isDone = true;
            return true;
        }
        catch (...) {
            return true; // 에러가 나도 완료 처리 (결과는 nil)
        }
    }
    return false;
}

sol::object JsonTask::getResult(sol::this_state s) {
    return wrap_json_node(*g_JsonCache[path], s);
}

// --- 모듈 등록 함수 ---
void register_json_module(sol::state_view& lua, const char* namespace_name) {
    std::unordered_map<std::string, std::unique_ptr<json>> empty;
    g_JsonCache.swap(empty);
    // 1. JsonNode 유저타입 등록
    lua.new_usertype<JsonNode>("json_node",
        sol::meta_function::index, [](JsonNode& n, sol::stack_object key, sol::this_state s) -> sol::object {
            if (!n.node) return sol::nil;
            auto& j = *(n.node);
            sol::state_view lua_s(s);

            if (key.is<std::string>() && j.is_object()) {
                auto it = j.find(key.as<std::string>());
                if (it != j.end()) return wrap_json_node(it.value(), lua_s);
            }
            else if (key.is<double>() && j.is_array()) {
                int idx = static_cast<int>(key.as<double>()) - 1;
                if (idx >= 0 && idx < (int)j.size()) return wrap_json_node(j[idx], lua_s);
            }
            return sol::nil;
        },
        sol::meta_function::length, [](JsonNode& n) {
            return (n.node && n.node->is_array()) ? n.node->size() : 0;
        },
        sol::meta_function::pairs, [](sol::this_state s, JsonNode& n) {
            sol::state_view lua_s(s);
            auto next_func = [](sol::this_state s, JsonNode& n, sol::object key) {
                sol::state_view l(s);
                if (!n.node) return std::make_tuple(sol::object(sol::nil), sol::object(sol::nil));
                auto& j = *(n.node);

                if (j.is_object()) {
                    auto it = key.is<sol::nil_t>() ? j.begin() : j.find(key.as<std::string>());
                    if (key.is<std::string>() && it != j.end()) ++it;
                    if (it != j.end()) return std::make_tuple(sol::make_object(l, it.key()), wrap_json_node(it.value(), l));
                }
                else if (j.is_array()) {
                    size_t idx = key.is<sol::nil_t>() ? 0 : static_cast<size_t>(key.as<double>());
                    if (idx < j.size()) return std::make_tuple(sol::make_object(l, idx + 1), wrap_json_node(j[idx], l));
                }
                return std::make_tuple(sol::object(sol::nil), sol::object(sol::nil));
                };
            return std::make_tuple(sol::make_object(lua_s, next_func), n, sol::nil);
        },
        sol::meta_function::to_string, [](JsonNode& n) {
            return n.node ? n.node->dump() : "nil node";
        }
    );

    // 2. 태이블에 로더 함수 추가
    sol::table res = lua[namespace_name].get_or_create<sol::table>();

    res["json"] = [](std::string path, sol::this_state s) -> sol::object {
        std::ifstream file(path);
        if (!file.is_open()) return sol::nil;

        try {
            std::lock_guard<std::mutex> lock(g_JsonMutex);
            g_JsonCache[path] = std::make_unique<json>();
            file >> *g_JsonCache[path];
            return wrap_json_node(*g_JsonCache[path], s);
        }
        catch (...) {
            return sol::nil;
        }
        };

    res["jsonAsync"] = [](std::string path) -> std::shared_ptr<ITask> {
        auto task = std::make_shared<JsonTask>();
        task->path = path;
        task->fuel = std::async(std::launch::async, [path]() {
            std::ifstream file(path);
            json j;
            if (file.is_open()) { try { file >> j; } catch (...) {} }
            return j;
            });
        return task;
        };
}