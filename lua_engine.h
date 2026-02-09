#pragma once
#include <windows.h>

#include <codeanalysis\warnings.h>
#pragma warning( push )
#pragma warning ( disable : ALL_CODE_ANALYSIS_WARNINGS )
#include <sol/sol.hpp>
#include <nlohmann/json.hpp>
#pragma warning( pop )

#include <future>
#include <fstream>
#include <gdiplus.h>
#include <string>
#include <vector>
#include <map>
#include <d2d1.h>
#include <dwrite.h>
#include <functional>
#include <wincodec.h> // 이미지 로딩을 위한 WIC
#include <wrl/client.h>
#include <comdef.h>

using json = nlohmann::json;
using Microsoft::WRL::ComPtr;

struct ITask {
    virtual ~ITask() = default;
    virtual bool check(sol::this_state s) = 0;
    virtual sol::object getResult() = 0;
    bool isDone = false;
};

// Lua가 들고 다닐 가벼운 객체
struct JsonNode {
    nlohmann::json* node = nullptr;
};
void RebuildAllBitmaps();