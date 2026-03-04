#pragma once
#include "includesol.h"
#include "luadef.h"
#include <string>
#include <vector>
#include <functional>
#include <type_traits>
#include <tuple>
#include <memory>

// =========================================================
// LuaBindContext - passed to each binding function
// =========================================================
struct LuaBindContext {
    sol::state& lua;
    LuaDefBuilder* def;  // null in release builds
    const char* name;    // namespace table name (e.g. "g", "res")
};

// =========================================================
// Type -> Lua type string mapping
// =========================================================
template<typename T> struct LuaTypeStr { static std::string get() { return "any"; } };
template<> struct LuaTypeStr<void> { static std::string get() { return ""; } };
template<> struct LuaTypeStr<int> { static std::string get() { return "integer"; } };
template<> struct LuaTypeStr<unsigned int> { static std::string get() { return "integer"; } };
template<> struct LuaTypeStr<long> { static std::string get() { return "integer"; } };
template<> struct LuaTypeStr<unsigned long> { static std::string get() { return "integer"; } };
template<> struct LuaTypeStr<float> { static std::string get() { return "number"; } };
template<> struct LuaTypeStr<double> { static std::string get() { return "number"; } };
template<> struct LuaTypeStr<bool> { static std::string get() { return "boolean"; } };
template<> struct LuaTypeStr<std::string> { static std::string get() { return "string"; } };
template<> struct LuaTypeStr<sol::table> { static std::string get() { return "table"; } };
template<> struct LuaTypeStr<sol::object> { static std::string get() { return "any"; } };
template<typename T> struct LuaTypeStr<std::optional<T>> {
    static std::string get() { return LuaTypeStr<T>::get() + "?"; }
};
template<typename T> struct LuaTypeStr<sol::optional<T>> {
    static std::string get() { return LuaTypeStr<T>::get() + "?"; }
};
template<typename T> struct LuaTypeStr<T*> { static std::string get() { return "any"; } };
template<typename T> struct LuaTypeStr<std::shared_ptr<T>> { static std::string get() { return "any"; } };

// =========================================================
// IsInjectedArg - Sol-injected params to skip in stubs
// =========================================================
template<typename T> struct IsInjectedArg : std::false_type {};
template<> struct IsInjectedArg<sol::this_state> : std::true_type {};
template<> struct IsInjectedArg<sol::this_environment> : std::true_type {};

// =========================================================
// FunctionTraits - extract return type and arg types
// =========================================================
template<typename T, typename Enable = void>
struct FunctionTraits;

// Functors / lambdas (class types with operator())
template<typename T>
struct FunctionTraits<T, std::enable_if_t<std::is_class_v<T>>>
    : FunctionTraits<decltype(&std::decay_t<T>::operator())> {
};

// Mutable member function pointer
template<typename C, typename R, typename... Args>
struct FunctionTraits<R(C::*)(Args...)> {
    using ReturnType = R;
    using ArgsTuple = std::tuple<Args...>;
};

// Const member function pointer
template<typename C, typename R, typename... Args>
struct FunctionTraits<R(C::*)(Args...) const> {
    using ReturnType = R;
    using ArgsTuple = std::tuple<Args...>;
};

// Free function pointer
template<typename R, typename... Args>
struct FunctionTraits<R(*)(Args...)> {
    using ReturnType = R;
    using ArgsTuple = std::tuple<Args...>;
};

// =========================================================
// Internal helpers
// =========================================================
namespace detail {

    template<typename Tuple, size_t N>
    struct ParamCollector {
        static void Collect(std::vector<std::string>& types) {
            if constexpr (N < std::tuple_size_v<Tuple>) {
                using RawT = std::decay_t<std::tuple_element_t<N, Tuple>>;
                if constexpr (!IsInjectedArg<RawT>::value) {
                    types.push_back(LuaTypeStr<RawT>::get());
                }
                ParamCollector<Tuple, N + 1>::Collect(types);
            }
        }
    };

    template<typename F>
    FuncDef BuildFuncDef(const char* funcName) {
        using DecayedF = std::decay_t<F>;
        using Traits = FunctionTraits<DecayedF>;
        using ArgsTuple = typename Traits::ArgsTuple;
        using RetType = typename Traits::ReturnType;

        std::vector<std::string> paramTypes;
        ParamCollector<ArgsTuple, 0>::Collect(paramTypes);

        FuncDef fd;
        fd.name = funcName;
        fd.returnType = LuaTypeStr<RetType>::get();
        for (size_t i = 0; i < paramTypes.size(); i++) {
            ParamDef p;
            p.name = "p" + std::to_string(i + 1);
            p.luaType = paramTypes[i];
            fd.params.push_back(p);
        }
        return fd;
    }

} // namespace detail

// =========================================================
// LuaNamespaceBinder - registers functions and records stubs
// =========================================================
class LuaNamespaceBinder {
public:
    explicit LuaNamespaceBinder(LuaBindContext& ctx)
        : m_ctx(ctx)
        , m_table(ctx.lua.create_named_table(ctx.name))
    {
    }

    template<typename F>
    LuaNamespaceBinder& func(const char* name, F&& f) {
        m_table[name] = f;
        if (m_ctx.def) {
            auto fd = detail::BuildFuncDef<std::decay_t<F>>(name);
            m_ctx.def->AddNamespaceFunc(m_ctx.name, fd);
        }
        return *this;
    }

    LuaNamespaceBinder& names(std::initializer_list<const char*> paramNames) {
        if (m_ctx.def) {
            std::vector<std::string> ns(paramNames.begin(), paramNames.end());
            m_ctx.def->UpdateLastFuncNames(m_ctx.name, ns);
        }
        return *this;
    }

    LuaNamespaceBinder& returns(const char* retType) {
        if (m_ctx.def) {
            m_ctx.def->UpdateLastFuncReturn(m_ctx.name, retType);
        }
        return *this;
    }

    LuaNamespaceBinder& desc(const char* description) {
        if (m_ctx.def) {
            m_ctx.def->UpdateLastFuncDesc(m_ctx.name, description);
        }
        return *this;
    }

    sol::table& table() { return m_table; }

private:
    LuaBindContext& m_ctx;
    sol::table m_table;
};

// =========================================================
// LuaClassBinder - records class methods for stub generation AND registers usertype
// =========================================================
class LuaClassBinder {
public:
    template<typename T>
    class Builder {
    public:
        Builder(LuaBindContext& ctx, const char* className)
            : m_ctx(ctx), m_def(ctx.def), m_className(className) {
            // Usertype 초기화 (메서드는 나중에 추가)
            m_usertype = m_ctx.lua.new_usertype<T>(className);
        }

        template<typename F>
        Builder& method(const char* name, F&& f) {
            // Sol usertype에 메서드 추가
            m_usertype[name] = f;
            
            // 문서화를 위한 FuncDef 등록
            if (m_def) {
                auto fd = detail::BuildFuncDef<std::decay_t<F>>(name);
                m_def->AddClassMethod(m_className, fd);
                m_lastMethod = name;
            }
            return *this;
        }

        Builder& names(std::initializer_list<const char*> paramNames) {
            if (m_def && !m_lastMethod.empty()) {
                std::vector<std::string> ns(paramNames.begin(), paramNames.end());
                m_def->UpdateLastMethodNames(m_className, ns);
            }
            return *this;
        }

        Builder& desc(const char* description) {
            if (m_def && !m_lastMethod.empty()) {
                m_def->UpdateLastMethodDesc(m_className, description);
            }
            return *this;
        }

    private:
        LuaBindContext& m_ctx;
        LuaDefBuilder* m_def;
        std::string m_className;
        std::string m_lastMethod;
        sol::usertype<T> m_usertype;
    };
};