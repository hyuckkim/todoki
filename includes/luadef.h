#pragma once
#include <string>
#include <vector>

struct ParamDef {
    std::string name;
    std::string luaType;
};

struct FuncDef {
    std::string name;
    std::vector<ParamDef> params;
    std::string returnType; // empty = void (no ---@return)
    std::string description; // function description comment
};

struct NamespaceDef {
    std::string name;
    std::vector<FuncDef> funcs;
};

struct ClassDef {
    std::string name;
    std::vector<FuncDef> methods;
};

// C++ -> Lua callable 함수 목록용
struct LuaCallableDef {
    FuncDef func;
};

class LuaDefBuilder {
public:
    void AddNamespaceFunc(const std::string& ns, const FuncDef& fd);
    void UpdateLastFuncNames(const std::string& ns, const std::vector<std::string>& names);
    void UpdateLastFuncReturn(const std::string& ns, const std::string& retType);
    void UpdateLastFuncDesc(const std::string& ns, const std::string& desc);

    void AddClassMethod(const std::string& cls, const FuncDef& fd);
    void UpdateLastMethodNames(const std::string& cls, const std::vector<std::string>& names);
    void UpdateLastMethodDesc(const std::string& cls, const std::string& desc);

    void Write(const char* path = "_globalDef.lua") const;

    // C++ -> Lua callable 함수 기록
    void AddLuaCallableFunc(const FuncDef& fd);

private:
    std::vector<NamespaceDef> m_namespaces;
    std::vector<ClassDef> m_classes;

    // C++ -> Lua callable 함수 목록
    std::vector<LuaCallableDef> m_luaCallables;

    NamespaceDef* FindOrAddNamespace(const std::string& ns);
    ClassDef* FindOrAddClass(const std::string& cls);
};