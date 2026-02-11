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

// JSON -> Lua Table 변환 (json_node가 아님!)
sol::object json_to_lua_table(const json& j, sol::state_view lua) {
    if (j.is_null()) return sol::nil;
    if (j.is_boolean()) return sol::make_object(lua, j.get<bool>());
    if (j.is_number()) return sol::make_object(lua, j.get<double>());
    if (j.is_string()) return sol::make_object(lua, j.get<std::string>());

    if (j.is_array()) {
        sol::table t = lua.create_table();
        for (size_t i = 0; i < j.size(); ++i) {
            t[i + 1] = json_to_lua_table(j[i], lua); // Lua 1-based index
        }
        return t;
    }
    if (j.is_object()) {
        sol::table t = lua.create_table();
        for (auto& el : j.items()) {
            t[el.key()] = json_to_lua_table(el.value(), lua);
        }
        return t;
    }
    return sol::nil;
}

// Lua Table -> JSON 변환 (json_node가 아님!)
json lua_table_to_json(sol::object obj) {
    if (obj.is<sol::nil_t>()) return nullptr;
    if (obj.is<bool>()) return obj.as<bool>();
    if (obj.is<double>()) return obj.as<double>();
    if (obj.is<std::string>()) return obj.as<std::string>();

    if (obj.is<sol::table>()) {
        sol::table t = obj.as<sol::table>();

        // 배열인지 객체인지 판별 (첫 번째 키가 1이면 배열로 추측)
        bool is_array = false;
        t.for_each([&is_array](sol::object key, sol::object value) {
            if (key.is<int>() && key.as<int>() == 1) is_array = true;
            return false; // 중단
            });

        if (is_array) {
            json j = json::array();
            for (size_t i = 1; i <= t.size(); ++i) {
                j.push_back(lua_table_to_json(t[i]));
            }
            return j;
        }
        else {
            json j = json::object();
            t.for_each([&j](sol::object key, sol::object value) {
                if (key.is<std::string>()) {
                    j[key.as<std::string>()] = lua_table_to_json(value);
                }
                });
            return j;
        }
    }
    return nullptr;
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

    res["loadTable"] = [](std::string path, sol::this_state s) -> sol::object {
        std::ifstream file(path);
        if (!file.is_open()) return sol::nil;
        try {
            json j;
            file >> j;
            return json_to_lua_table(j, s);
        }
        catch (...) { return sol::nil; }
        };

    res["saveTable"] = [](std::string path, sol::object table) -> bool {
        std::ofstream file(path);
        if (!file.is_open()) return false;
        try {
            json j = lua_table_to_json(table);
            file << j.dump(4); // 4칸 들여쓰기 포함
            return true;
        }
        catch (...) { return false; }
        };
}