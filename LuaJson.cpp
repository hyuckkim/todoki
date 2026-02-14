#include "luajson.h"
#include <fstream>

using json = nlohmann::json;

sol::object wrap_json_node(std::shared_ptr<json> root, json* current, sol::state_view lua) {
    if (current->is_structured()) {
        return sol::make_object(lua, JsonNode{ root, current });
    }

    if (current->is_string()) return sol::make_object(lua, current->get<std::string>());
    if (current->is_number()) return sol::make_object(lua, current->get<double>());
    if (current->is_boolean()) return sol::make_object(lua, current->get<bool>());
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

    // 비동기 작업이 완료되었는지 확인
    if (fuel.valid() && fuel.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        try {
            // 결과를 unique_ptr이 아닌 shared_ptr로 받아옵니다.
            // fuel.get()은 한 번만 호출 가능하므로 바로 할당합니다.
            result = std::make_shared<json>(fuel.get());
            isDone = true;
            return true;
        }
        catch (...) {
            result = nullptr; // 실패 시 안전하게 초기화
            isDone = true;
            return true;
        }
    }
    return false;
}

sol::object JsonTask::getResult(sol::this_state s) {
    if (!result) return sol::nil;

    sol::state_view lua(s);
    return wrap_json_node(result, result.get(), lua);
}

sol::object json_to_lua(nlohmann::json& j, sol::state_view& lua) {
    if (j.is_null()) return sol::nil;
    if (j.is_boolean()) return sol::make_object(lua, j.get<bool>());
    if (j.is_number()) return sol::make_object(lua, j.get<double>());
    if (j.is_string()) return sol::make_object(lua, j.get<std::string>());

    if (j.is_object()) {
        sol::table t = lua.create_table();
        for (auto& el : j.items()) {
            t[el.key()] = json_to_lua(el.value(), lua);
        }
        return t;
    }

    if (j.is_array()) {
        sol::table t = lua.create_table();
        for (size_t i = 0; i < j.size(); ++i) {
            t[i + 1] = json_to_lua(j[i], lua);
        }
        return t;
    }
    return sol::nil;
}

// --- 모듈 등록 함수 ---
void register_json_module(sol::state_view& lua, const char* namespace_name) {
    // 1. JsonNode 유저타입 등록
    lua.new_usertype<JsonNode>("json_node",
        sol::meta_function::index, [](JsonNode& n, sol::stack_object key, sol::this_state s) -> sol::object {
            if (!n.node) return sol::nil;
            auto& j = *(n.node);
            sol::state_view lua_s(s);

            if (key.is<std::string>() && j.is_object()) {
                auto it = j.find(key.as<std::string>());
                if (it != j.end()) return wrap_json_node(n.data, &it.value(), lua_s);
            }
            else if (key.is<double>() && j.is_array()) {
                int idx = static_cast<int>(key.as<double>()) - 1;
                if (idx >= 0 && idx < (int)j.size()) return wrap_json_node(n.data, &j[idx], lua_s);
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
                    if (it != j.end()) return std::make_tuple(sol::make_object(l, it.key()), wrap_json_node(n.data, &it.value(), l));
                }
                else if (j.is_array()) {
                    size_t idx = key.is<sol::nil_t>() ? 0 : static_cast<size_t>(key.as<double>());
                    if (idx < j.size()) return std::make_tuple(sol::make_object(l, idx + 1), wrap_json_node(n.data, &j[idx], l));
                }
                return std::make_tuple(sol::object(sol::nil), sol::object(sol::nil));
                };
            return std::make_tuple(sol::make_object(lua_s, next_func), n, sol::nil);
        },
        sol::meta_function::to_string, [](JsonNode& n) {
            return n.node ? n.node->dump() : "nil node";
        },
        "to_table", [](JsonNode& n, sol::this_state s) {
            sol::state_view lua_s(s);
            return n.node ? json_to_lua(*(n.node), lua_s) : sol::object(sol::nil);
        }
    );

    // 2. 태이블에 로더 함수 추가
    sol::table res = lua[namespace_name].get_or_create<sol::table>();

    res["json"] = [](std::string path, sol::this_state s) -> sol::object {
        std::ifstream file(path);
        if (!file.is_open()) return sol::nil;

        try {
            auto j = std::make_shared<json>(); // 매번 새로 생성
            file >> *j;
            return wrap_json_node(j, j.get(), s); // shared_ptr을 넘겨서 Lua가 소유하게 함
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