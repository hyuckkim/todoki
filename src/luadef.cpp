#include "luadef.h"
#include <fstream>
#include <algorithm>

void LuaDefBuilder::AddNamespaceFunc(const std::string& ns, const FuncDef& fd) {
    NamespaceDef* nsDef = FindOrAddNamespace(ns);
    nsDef->funcs.push_back(fd);
}

void LuaDefBuilder::UpdateLastFuncNames(const std::string& ns, const std::vector<std::string>& names) {
    NamespaceDef* nsDef = FindOrAddNamespace(ns);
    if (nsDef->funcs.empty()) return;
    
    FuncDef& fd = nsDef->funcs.back();
    size_t minSize = std::min(names.size(), fd.params.size());
    for (size_t i = 0; i < minSize; i++) {
        fd.params[i].name = names[i];
    }
}

void LuaDefBuilder::UpdateLastFuncReturn(const std::string& ns, const std::string& retType) {
    NamespaceDef* nsDef = FindOrAddNamespace(ns);
    if (nsDef->funcs.empty()) return;
    
    nsDef->funcs.back().returnType = retType;
}

void LuaDefBuilder::UpdateLastFuncDesc(const std::string& ns, const std::string& desc) {
    NamespaceDef* nsDef = FindOrAddNamespace(ns);
    if (nsDef->funcs.empty()) return;
    
    nsDef->funcs.back().description = desc;
}

void LuaDefBuilder::AddClassMethod(const std::string& cls, const FuncDef& fd) {
    ClassDef* clsDef = FindOrAddClass(cls);
    clsDef->methods.push_back(fd);
}

void LuaDefBuilder::UpdateLastMethodNames(const std::string& cls, const std::vector<std::string>& names) {
    ClassDef* clsDef = FindOrAddClass(cls);
    if (clsDef->methods.empty()) return;
    
    FuncDef& fd = clsDef->methods.back();
    size_t minSize = std::min(names.size(), fd.params.size());
    for (size_t i = 0; i < minSize; i++) {
        fd.params[i].name = names[i];
    }
}

void LuaDefBuilder::Write(const char* path) const {
    std::ofstream out(path);
    if (!out) return;

    out << "---@meta\n\n";

    // Write namespaces
    for (const auto& ns : m_namespaces) {
        out << "---@class " << ns.name << "\n";
        out << ns.name << " = {}\n\n";

        for (const auto& func : ns.funcs) {
            // Write description if available
            if (!func.description.empty()) {
                out << "--- " << func.description << "\n";
            }
            
            // Write param annotations
            for (const auto& param : func.params) {
                out << "---@param " << param.name << " " << param.luaType << "\n";
            }
            
            // Write return annotation
            if (!func.returnType.empty()) {
                out << "---@return " << func.returnType << "\n";
            }
            
            // Write function signature
            out << "function " << ns.name << "." << func.name << "(";
            for (size_t i = 0; i < func.params.size(); i++) {
                if (i > 0) out << ", ";
                out << func.params[i].name;
            }
            out << ") end\n\n";
        }
    }

    // Write classes
    for (const auto& cls : m_classes) {
        out << "---@class " << cls.name << "\n";
        out << cls.name << " = {}\n\n";

        for (const auto& method : cls.methods) {
            // Write description if available
            if (!method.description.empty()) {
                out << "--- " << method.description << "\n";
            }
            
            // Write param annotations
            for (const auto& param : method.params) {
                out << "---@param " << param.name << " " << param.luaType << "\n";
            }
            
            // Write return annotation
            if (!method.returnType.empty()) {
                out << "---@return " << method.returnType << "\n";
            }
            
            // Write method signature
            out << "function " << cls.name << ":" << method.name << "(";
            for (size_t i = 0; i < method.params.size(); i++) {
                if (i > 0) out << ", ";
                out << method.params[i].name;
            }
            out << ") end\n\n";
        }
    }

    out.close();
}

NamespaceDef* LuaDefBuilder::FindOrAddNamespace(const std::string& ns) {
    auto it = std::find_if(m_namespaces.begin(), m_namespaces.end(),
        [&ns](const NamespaceDef& n) { return n.name == ns; });
    
    if (it != m_namespaces.end()) {
        return &(*it);
    }
    
    m_namespaces.push_back(NamespaceDef{ns, {}});
    return &m_namespaces.back();
}

ClassDef* LuaDefBuilder::FindOrAddClass(const std::string& cls) {
    auto it = std::find_if(m_classes.begin(), m_classes.end(),
        [&cls](const ClassDef& c) { return c.name == cls; });
    
    if (it != m_classes.end()) {
        return &(*it);
    }
    
    m_classes.push_back(ClassDef{cls, {}});
    return &m_classes.back();
}
