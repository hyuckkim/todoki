#include "lua_engine.h"

// 전역 변수 초기화 (기존 유지)
Color g_currentColor(255, 255, 255, 255);
std::vector<Image*> g_imageTable;
std::vector<Font*> g_fontTable;
std::map<std::string, int> g_pathCache;

// sol2에서는 sol::state가 알아서 죽을 때 정리하지만, 
// GDI+ 객체(new로 생성한 것)들은 명시적으로 지워주는 게 좋습니다.
void unregisterLuaFunctions() {
    for (auto img : g_imageTable) delete img;
    for (auto font : g_fontTable) delete font;
    g_fontTable.clear();
    g_imageTable.clear();
    g_pathCache.clear();
}

void register_res(sol::state& lua, const char* name) {
    auto res = lua.create_named_table(name);

    // 1. 이미지 로드 (캐싱 로직 포함)
    res["image"] = [](std::string path) -> int {
        auto it = g_pathCache.find(path);
        if (it != g_pathCache.end()) return it->second;

        std::wstring wPath = to_wstring(path);
        Image* img = Image::FromFile(wPath.c_str());

        int newID = (int)g_imageTable.size();
        g_imageTable.push_back(img);
        g_pathCache[path] = newID;
        return newID;
        };

    // 2. 시스템 폰트 로드
    res["font"] = [](std::string name, float size, sol::optional<int> style) -> int {
        std::wstring wName = to_wstring(name);
        Font* font = new Font(wName.c_str(), size, style.value_or(FontStyleRegular));

        int id = (int)g_fontTable.size();
        g_fontTable.push_back(font);
        return id;
        };

    // 3. 폰트 파일(.ttf) 로드
    res["fontFile"] = [](std::string path, float size) -> int {
        char fullPath[MAX_PATH];
        GetFullPathNameA(path.c_str(), MAX_PATH, fullPath, NULL);
        std::wstring wPath = to_wstring(fullPath);

        if (g_pfc->AddFontFile(wPath.c_str()) != Ok) return -1;

        int count = g_pfc->GetFamilyCount();
        if (count == 0) return -1;

        std::unique_ptr<FontFamily[]> families(new FontFamily[count]);
        int found = 0;
        g_pfc->GetFamilies(count, families.get(), &found);

        Font* font = new Font(&families[count - 1], size, FontStyleRegular, UnitPixel);
        int id = (int)g_fontTable.size();
        g_fontTable.push_back(font);
        return id;
        };

    // 4. 드디어 대망의 JSON 로더 (여기에 꽂으시면 됩니다)
    res["json"] = [&lua](std::string path) -> sol::object {
        // nlohmann/json 등을 써서 파일을 읽은 뒤 
        // sol::table로 변환해서 반환할 예정입니다.
        // 지금은 일단 nil을 반환하게 둡니다.
        return sol::nil;
        };
}