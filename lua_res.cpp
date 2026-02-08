#include "lua_engine.h"

// 전역 변수 초기화 (기존 유지)
Color g_currentColor(255, 255, 255, 255);
std::vector<ID2D1Bitmap*> g_bitmapTable;
std::map<std::string, int> g_pathCache;
std::vector<IDWriteTextFormat*> g_fontTable;
std::vector<std::wstring> g_fontFamilyTable;
static std::unordered_map<std::string, std::unique_ptr<nlohmann::json>> g_JsonCache;
static std::mutex g_JsonMutex;

ID2D1Bitmap* LoadBitmapFromFile(
    ID2D1DCRenderTarget* rt,
    const std::string& path
) {
    std::wstring wPath = to_wstring(path);

    IWICBitmapDecoder* pDecoder = nullptr;
    HRESULT hr = g_pWICFactory->CreateDecoderFromFilename(
        wPath.c_str(),
        NULL,
        GENERIC_READ,
        WICDecodeMetadataCacheOnLoad,
        &pDecoder
    );

    if (FAILED(hr)) {
        if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) {
            printf("[Resource Error] File not found: %s\n", path.c_str());
        }
        else {
            printf("[Resource Error] Failed to load '%s' (HRESULT: 0x%08X)\n", path.c_str(), hr);
        }
        return nullptr;
    }

    IWICBitmapFrameDecode* pSource = nullptr;
    pDecoder->GetFrame(0, &pSource);

    IWICFormatConverter* pConverter = nullptr;
    g_pWICFactory->CreateFormatConverter(&pConverter);
    pConverter->Initialize(
        pSource,
        GUID_WICPixelFormat32bppPBGRA,
        WICBitmapDitherTypeNone,
        NULL,
        0.f,
        WICBitmapPaletteTypeMedianCut
    );

    ID2D1Bitmap* pBitmap = nullptr;
    rt->CreateBitmapFromWicBitmap(pConverter, NULL, &pBitmap);

    pConverter->Release();
    pSource->Release();
    pDecoder->Release();

    return pBitmap; // 실패 시 nullptr 가능
}

void unregisterLuaFunctions() {
    for (auto img : g_bitmapTable) if (img) img->Release();
    for (auto font : g_fontTable) if (font) font->Release();
    for (auto& fontPath : g_fontFamilyTable) {
        RemoveFontResourceExW(fontPath.c_str(), FR_PRIVATE, 0);
	}
    g_fontTable.clear();
    g_bitmapTable.clear();
    g_pathCache.clear();
}

void RebuildAllBitmaps() {
    for (auto& bmp : g_bitmapTable) {
        SafeRelease(&bmp);
    }
    for (const auto& [path, index]: g_pathCache) {
        if (index < 0 || index >= (int)g_bitmapTable.size())
            continue; // 방어

        ID2D1Bitmap* bmp = LoadBitmapFromFile(g_pDCRT, path);
        g_bitmapTable[index] = bmp; // 실패 시 nullptr
    }
}

sol::object wrap_json_node(nlohmann::json& j, sol::state_view lua) {
    if (j.is_structured()) {
        return sol::make_object<JsonNode>(lua, JsonNode{ &j });
    }
    if (j.is_string())  return sol::make_object(lua, j.get<std::string>());
    if (j.is_number())  return sol::make_object(lua, j.get<double>());
    if (j.is_boolean()) return sol::make_object(lua, j.get<bool>());
    return sol::nil;
}
struct JsonTask : public ITask {
    std::string path;
    std::future<nlohmann::json> fuel;
    sol::object result = sol::nil;

    bool check(sol::this_state s) override {
        if (isDone) return true;

        if (fuel.valid() && fuel.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            std::lock_guard<std::mutex> lock(g_JsonMutex);

            // 1. 전역 캐시에 데이터 저장
            g_JsonCache[path] = std::make_unique<nlohmann::json>(fuel.get());

            // 2. 결과 Proxy 객체 미리 생성 (getResult가 매개변수가 없으므로)
            sol::state_view lua(s);
            result = sol::make_object<JsonNode>(lua, JsonNode{ g_JsonCache[path].get() });

            isDone = true;
            return true;
        }
        return false;
    }

    sol::object getResult() override {
        return result;
    }
};
// 반복자(next) 함수 정의
auto json_next = [](sol::this_state s, JsonNode& n, sol::object key) {
    sol::state_view lua(s);
    if (!n.node) return std::make_tuple(sol::object(sol::nil), sol::object(sol::nil));

    auto& j = *(n.node);

    // 1. 객체(Object) 순회
    if (j.is_object()) {
        auto it = j.end();
        if (key.is<sol::nil_t>()) {
            it = j.begin();
        }
        else if (key.is<std::string>()) {
            it = j.find(key.as<std::string>());
            if (it != j.end()) ++it;
        }

        if (it != j.end()) {
            return std::make_tuple(sol::make_object(lua, it.key()), wrap_json_node(it.value(), lua));
        }
    }
    // 2. 배열(Array) 순회
    else if (j.is_array()) {
        size_t next_idx = 0;
        if (key.is<sol::nil_t>()) {
            next_idx = 0;
        }
        else if (key.is<double>()) {
            next_idx = static_cast<size_t>(key.as<double>()); // 루아에서 보낸 index (1-based 기준 그대로가 다음 순서)
        }

        if (next_idx < j.size()) {
            return std::make_tuple(sol::make_object(lua, next_idx + 1), wrap_json_node(j[next_idx], lua));
        }
    }

    return std::make_tuple(sol::object(sol::nil), sol::object(sol::nil));
    };

auto json_ipairs_next = [](sol::this_state s, JsonNode& n, sol::object key) {
    sol::state_view lua(s);
    if (!n.node || !n.node->is_array())
        return std::make_tuple(sol::object(sol::nil), sol::object(sol::nil));

    auto& j = *(n.node);
    int next_idx = 0;

    if (key.is<sol::nil_t>()) {
        next_idx = 0; // 시작
    }
    else {
        next_idx = static_cast<int>(key.as<double>()); // 현재 index 1이면 다음은 인덱스 1(0-based)
    }

    if (next_idx < (int)j.size()) {
        return std::make_tuple(
            sol::make_object(lua, next_idx + 1), // 다음 루아 인덱스
            wrap_json_node(j[next_idx], lua)     // 값
        );
    }

    return std::make_tuple(sol::object(sol::nil), sol::object(sol::nil));
    };
void register_json_type(sol::state_view& lua) {
    lua.new_usertype<JsonNode>("json_node",
        // __index: 객체의 키(string) 또는 배열의 인덱스(double/int) 처리
        sol::meta_function::index, [](JsonNode& n, sol::stack_object key, sol::this_state s) -> sol::object {
            if (!n.node) return sol::nil;
            auto& j = *(n.node);
            sol::state_view lua_s(s);

            if (key.is<std::string>() && j.is_object()) {
                auto it = j.find(key.as<std::string>());
                if (it != j.end()) return wrap_json_node(it.value(), lua_s);
            }
            // 루아 숫자는 double이므로 double로 체크하는 것이 안전함
            else if (key.is<double>() && j.is_array()) {
                int idx = static_cast<int>(key.as<double>()) - 1;
                if (idx >= 0 && idx < (int)j.size()) {
                    return wrap_json_node(j[idx], lua_s);
                }
            }
            return sol::nil;
        },
        // __len: 표준 루아처럼 객체일 때는 0, 배열일 때만 크기 반환
        sol::meta_function::length, [](JsonNode& n) {
            if (n.node && n.node->is_array()) {
                return n.node->size();
            }
            return (size_t)0; // 객체(Map)는 루아 표준에서 # 연산시 0임
        },

        // __pairs: 객체와 배열 모두 순회
        sol::meta_function::pairs, [](sol::this_state s, JsonNode& n) {
            sol::state_view lua(s);
            return std::make_tuple(sol::make_object(lua, json_next), n, sol::nil);
        },

        sol::meta_function::ipairs, [](sol::this_state s, JsonNode& n, sol::object key) {
            sol::state_view lua(s);
            auto& j = *(n.node);
            int next_idx = 0;

            if (key.is<sol::nil_t>()) {
                next_idx = 0;
            }
            else {
                next_idx = static_cast<int>(key.as<double>());
            }
            return std::make_tuple(
                sol::make_object(lua, next_idx + 1), // JsonNode가 아닌 순수 number
                wrap_json_node(j[next_idx], lua)     // Value는 JsonNode여도 됨
            );
        },
        sol::meta_function::to_string, [](JsonNode& n) {
            if (n.node) return n.node->dump(); // JSON을 문자열로 직렬화
            return std::string("nil node");
            }
    );
}

void register_res(sol::state& lua, const char* name) {
    lua.new_usertype<ITask>("Task",
        "check", &ITask::check,
        "getResult", &ITask::getResult,
        "isDone", sol::readonly(&ITask::isDone)
    );
    register_json_type(lua);

    auto res = lua.create_named_table(name);

    // 1. 이미지 로드 (캐싱 로직 포함)
    res["image"] = [](std::string path) -> int {
        auto it = g_pathCache.find(path);
        if (it != g_pathCache.end())
            return it->second;

        ID2D1Bitmap* pBitmap = LoadBitmapFromFile(g_pDCRT, path);
        if (!pBitmap)
            return -1;

        int newID = (int)g_bitmapTable.size();
        g_bitmapTable.push_back(pBitmap);
        g_pathCache[path] = newID;
        return newID;
        };


    // 2. 시스템 폰트 로드
    res["font"] = [](std::string name, float size, sol::optional<int> weight) -> int {
        std::wstring wName = to_wstring(name);

        IDWriteTextFormat* pTextFormat = nullptr;
        // weight: DWRITE_FONT_WEIGHT_NORMAL (400) 등 사용
        g_pDWriteFactory->CreateTextFormat(
            wName.c_str(), NULL,
            (DWRITE_FONT_WEIGHT)weight.value_or(400),
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            size, L"ko-kr", &pTextFormat
        );

        int id = (int)g_fontTable.size();
        g_fontTable.push_back(pTextFormat);
        return id;
        };

    // 3. 폰트 파일(.ttf) 로드
    res["fontFile"] = [](std::string path, std::string familyName, float size) -> int {
        // 1. 파일 존재 여부 확인 (기본적인 가드)
        std::wstring wPath = to_wstring(path);
        std::wstring wName = to_wstring(familyName);

        // 2. OS에 폰트 등록 시도
        // 반환값이 0이면 등록 실패 (파일이 없거나 형식이 잘못됨)
        int fontsAdded = AddFontResourceExW(wPath.c_str(), FR_PRIVATE, 0);

        if (fontsAdded == 0) {
            printf("[Resource Error] Font file not found or invalid: %s\n", path.c_str());
            // 실패 시 -1 반환 혹은 기본 폰트 처리
            return -1;
        }

        g_fontFamilyTable.push_back(wPath);

        // 3. TextFormat 생성
        IDWriteTextFormat* pTextFormat = nullptr;
        HRESULT hr = g_pDWriteFactory->CreateTextFormat(
            wName.c_str(),
            nullptr,
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            size, L"ko-kr", &pTextFormat
        );

        if (FAILED(hr)) {
            printf("[Resource Error] Failed to create TextFormat for: %s (HRESULT: 0x%08X)\n", familyName.c_str(), hr);
            // 등록했던 리소스 해제
            RemoveFontResourceExW(wPath.c_str(), FR_PRIVATE, 0);
            return -1;
        }

        int id = (int)g_fontTable.size();
        g_fontTable.push_back(pTextFormat);

        return id;
        };

    // 4. 드디어 대망의 JSON 로더 (여기에 꽂으시면 됩니다)
    res["json"] = [&lua](std::string path) -> sol::object {
        std::ifstream file(path);
        if (!file.is_open()) {
            printf("[Resource Error] Failed to open JSON file: %s\n", path.c_str());
            return sol::nil;
        }

        json j;
        try {
            file >> j;
        }
        catch (const json::parse_error& e) {
            printf("[JSON Error] Parse error: %s\n", e.what());
            return sol::nil;
        }

        // 전역 lua 상태를 사용하여 변환
        sol::state_view lua_view(lua);
        return wrap_json_node(j, lua);
        }; 
    res["jsonAsync"] = [](std::string path) -> std::shared_ptr<ITask> {
        auto task = std::make_shared<JsonTask>();
        task->path = path; // 캐시 키로 사용

        task->fuel = std::async(std::launch::async, [path]() {
            std::ifstream file(path);
            nlohmann::json j;
            if (file.is_open()) {
                try {
                    file >> j;
                }
                catch (...) {
                    // 파싱 에러 처리 로직 (빈 객체 반환 등)
                }
            }
            return j;
            });

        return task;
        };
}