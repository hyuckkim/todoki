#include "luadef.h"
#include <cstdio>

NamespaceDef* LuaDefBuilder::FindOrAddNamespace(const std::string& ns) {
    for (auto& nd : m_namespaces) {
        if (nd.name == ns) return &nd;
    }
    m_namespaces.push_back({ns, {}});
    return &m_namespaces.back();
}

ClassDef* LuaDefBuilder::FindOrAddClass(const std::string& cls) {
    for (auto& cd : m_classes) {
        if (cd.name == cls) return &cd;
    }
    m_classes.push_back({cls, {}});
    return &m_classes.back();
}

void LuaDefBuilder::AddNamespaceFunc(const std::string& ns, const FuncDef& fd) {
    FindOrAddNamespace(ns)->funcs.push_back(fd);
}

void LuaDefBuilder::UpdateLastFuncNames(const std::string& ns, const std::vector<std::string>& names) {
    auto* nd = FindOrAddNamespace(ns);
    if (nd->funcs.empty()) return;
    auto& fd = nd->funcs.back();
    for (size_t i = 0; i < names.size() && i < fd.params.size(); i++) {
        fd.params[i].name = names[i];
    }
}

void LuaDefBuilder::UpdateLastFuncReturn(const std::string& ns, const std::string& retType) {
    auto* nd = FindOrAddNamespace(ns);
    if (nd->funcs.empty()) return;
    nd->funcs.back().returnType = retType;
}

void LuaDefBuilder::AddClassMethod(const std::string& cls, const FuncDef& fd) {
    FindOrAddClass(cls)->methods.push_back(fd);
}

void LuaDefBuilder::UpdateLastMethodNames(const std::string& cls, const std::vector<std::string>& names) {
    auto* cd = FindOrAddClass(cls);
    if (cd->methods.empty()) return;
    auto& fd = cd->methods.back();
    for (size_t i = 0; i < names.size() && i < fd.params.size(); i++) {
        fd.params[i].name = names[i];
    }
}

static std::string BuildParamList(const std::vector<ParamDef>& params) {
    std::string result;
    for (size_t i = 0; i < params.size(); i++) {
        if (i > 0) result += ", ";
        result += params[i].name;
    }
    return result;
}

void LuaDefBuilder::Write(const char* path) const {
    FILE* f = fopen(path, "w");
    if (!f) {
        printf("[LuaDef] Failed to open %s for writing\n", path);
        return;
    }

    fprintf(f, "-- Auto-generated Lua Language Server stubs\n");
    fprintf(f, "-- Do not edit manually\n\n");

    // Emit class definitions
    for (const auto& cd : m_classes) {
        fprintf(f, "---@class %s\n", cd.name.c_str());
        fprintf(f, "local %s = {}\n\n", cd.name.c_str());

        for (const auto& fd : cd.methods) {
            for (const auto& p : fd.params) {
                fprintf(f, "---@param %s %s\n", p.name.c_str(), p.luaType.c_str());
            }
            if (!fd.returnType.empty()) {
                fprintf(f, "---@return %s\n", fd.returnType.c_str());
            }
            fprintf(f, "function %s:%s(%s) end\n\n",
                cd.name.c_str(), fd.name.c_str(),
                BuildParamList(fd.params).c_str());
        }
    }

    // Emit namespace table definitions
    for (const auto& nd : m_namespaces) {
        fprintf(f, "%s = {}\n\n", nd.name.c_str());

        for (const auto& fd : nd.funcs) {
            for (const auto& p : fd.params) {
                fprintf(f, "---@param %s %s\n", p.name.c_str(), p.luaType.c_str());
            }
            if (!fd.returnType.empty()) {
                fprintf(f, "---@return %s\n", fd.returnType.c_str());
            }
            fprintf(f, "function %s.%s(%s) end\n\n",
                nd.name.c_str(), fd.name.c_str(),
                BuildParamList(fd.params).c_str());
        }
    }

    fclose(f);
    printf("[LuaDef] Written %s\n", path);
}
