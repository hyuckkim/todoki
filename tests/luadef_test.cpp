#include <gtest/gtest.h>
#include "luadef.h"
#include <sstream>
#include <fstream>
#include <stdexcept>
#include <cstdio>

// ----------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------
static std::string ReadFile(const char* path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        throw std::runtime_error(std::string("ReadFile: could not open ") + path);
    }
    return std::string(std::istreambuf_iterator<char>(f),
                       std::istreambuf_iterator<char>());
}

// ----------------------------------------------------------------
// LuaDefBuilder – namespace functions
// ----------------------------------------------------------------

TEST(LuaDefBuilder, AddNamespaceFuncWritesAnnotation) {
    LuaDefBuilder builder;
    FuncDef fd;
    fd.name = "doSomething";
    fd.params = {{"x", "number"}, {"y", "number"}};
    fd.returnType = "boolean";
    fd.description = "Does something";
    builder.AddNamespaceFunc("g", fd);

    const char* tmp = "_test_ns.lua";
    builder.Write(tmp);
    std::string content = ReadFile(tmp);
    std::remove(tmp);

    EXPECT_NE(content.find("---@class g"), std::string::npos);
    EXPECT_NE(content.find("--- Does something"), std::string::npos);
    EXPECT_NE(content.find("---@param x number"), std::string::npos);
    EXPECT_NE(content.find("---@param y number"), std::string::npos);
    EXPECT_NE(content.find("---@return boolean"), std::string::npos);
    EXPECT_NE(content.find("function g.doSomething(x, y) end"), std::string::npos);
}

TEST(LuaDefBuilder, UpdateLastFuncNamesRenamesParams) {
    LuaDefBuilder builder;
    FuncDef fd;
    fd.name = "move";
    fd.params = {{"p1", "number"}, {"p2", "number"}};
    builder.AddNamespaceFunc("sys", fd);
    builder.UpdateLastFuncNames("sys", {"dx", "dy"});

    const char* tmp = "_test_names.lua";
    builder.Write(tmp);
    std::string content = ReadFile(tmp);
    std::remove(tmp);

    EXPECT_NE(content.find("---@param dx number"), std::string::npos);
    EXPECT_NE(content.find("---@param dy number"), std::string::npos);
    EXPECT_EQ(content.find("---@param p1"), std::string::npos);
}

TEST(LuaDefBuilder, UpdateLastFuncReturnSetsReturnType) {
    LuaDefBuilder builder;
    FuncDef fd;
    fd.name = "getCount";
    builder.AddNamespaceFunc("res", fd);
    builder.UpdateLastFuncReturn("res", "integer");

    const char* tmp = "_test_ret.lua";
    builder.Write(tmp);
    std::string content = ReadFile(tmp);
    std::remove(tmp);

    EXPECT_NE(content.find("---@return integer"), std::string::npos);
}

TEST(LuaDefBuilder, UpdateLastFuncDescSetsDescription) {
    LuaDefBuilder builder;
    FuncDef fd;
    fd.name = "play";
    builder.AddNamespaceFunc("snd", fd);
    builder.UpdateLastFuncDesc("snd", "Plays a sound");

    const char* tmp = "_test_desc.lua";
    builder.Write(tmp);
    std::string content = ReadFile(tmp);
    std::remove(tmp);

    EXPECT_NE(content.find("--- Plays a sound"), std::string::npos);
}

TEST(LuaDefBuilder, UpdateOnEmptyNamespaceDoesNotCrash) {
    LuaDefBuilder builder;
    // Calling update methods on a namespace with no funcs should be a no-op
    EXPECT_NO_THROW(builder.UpdateLastFuncNames("ns", {"a"}));
    EXPECT_NO_THROW(builder.UpdateLastFuncReturn("ns", "string"));
    EXPECT_NO_THROW(builder.UpdateLastFuncDesc("ns", "desc"));
}

TEST(LuaDefBuilder, MultipleNamespacesWrittenSeparately) {
    LuaDefBuilder builder;
    FuncDef fd1; fd1.name = "alpha";
    FuncDef fd2; fd2.name = "beta";
    builder.AddNamespaceFunc("ns1", fd1);
    builder.AddNamespaceFunc("ns2", fd2);

    const char* tmp = "_test_multi.lua";
    builder.Write(tmp);
    std::string content = ReadFile(tmp);
    std::remove(tmp);

    EXPECT_NE(content.find("---@class ns1"), std::string::npos);
    EXPECT_NE(content.find("function ns1.alpha()"), std::string::npos);
    EXPECT_NE(content.find("---@class ns2"), std::string::npos);
    EXPECT_NE(content.find("function ns2.beta()"), std::string::npos);
}

// ----------------------------------------------------------------
// LuaDefBuilder – class methods
// ----------------------------------------------------------------

TEST(LuaDefBuilder, AddClassMethodWritesAnnotation) {
    LuaDefBuilder builder;
    FuncDef fd;
    fd.name = "draw";
    fd.params = {{"x", "number"}, {"y", "number"}};
    fd.returnType = "";
    fd.description = "Draw at position";
    builder.AddClassMethod("DrawContext", fd);

    const char* tmp = "_test_cls.lua";
    builder.Write(tmp);
    std::string content = ReadFile(tmp);
    std::remove(tmp);

    EXPECT_NE(content.find("---@class DrawContext"), std::string::npos);
    EXPECT_NE(content.find("--- Draw at position"), std::string::npos);
    EXPECT_NE(content.find("---@param x number"), std::string::npos);
    EXPECT_NE(content.find("---@param y number"), std::string::npos);
    // No return annotation for void
    EXPECT_EQ(content.find("---@return"), std::string::npos);
    EXPECT_NE(content.find("function DrawContext:draw(x, y) end"), std::string::npos);
}

TEST(LuaDefBuilder, UpdateLastMethodNamesRenamesParams) {
    LuaDefBuilder builder;
    FuncDef fd;
    fd.name = "resize";
    fd.params = {{"p1", "number"}, {"p2", "number"}};
    builder.AddClassMethod("Canvas", fd);
    builder.UpdateLastMethodNames("Canvas", {"width", "height"});

    const char* tmp = "_test_mnames.lua";
    builder.Write(tmp);
    std::string content = ReadFile(tmp);
    std::remove(tmp);

    EXPECT_NE(content.find("---@param width number"), std::string::npos);
    EXPECT_NE(content.find("---@param height number"), std::string::npos);
}

TEST(LuaDefBuilder, UpdateLastMethodDescSetsDescription) {
    LuaDefBuilder builder;
    FuncDef fd;
    fd.name = "clear";
    builder.AddClassMethod("Canvas", fd);
    builder.UpdateLastMethodDesc("Canvas", "Clears the canvas");

    const char* tmp = "_test_mdesc.lua";
    builder.Write(tmp);
    std::string content = ReadFile(tmp);
    std::remove(tmp);

    EXPECT_NE(content.find("--- Clears the canvas"), std::string::npos);
}

TEST(LuaDefBuilder, UpdateOnEmptyClassDoesNotCrash) {
    LuaDefBuilder builder;
    EXPECT_NO_THROW(builder.UpdateLastMethodNames("Foo", {"a"}));
    EXPECT_NO_THROW(builder.UpdateLastMethodDesc("Foo", "desc"));
}

TEST(LuaDefBuilder, WriteCreatesMetaHeader) {
    LuaDefBuilder builder;
    const char* tmp = "_test_meta.lua";
    builder.Write(tmp);
    std::string content = ReadFile(tmp);
    std::remove(tmp);

    EXPECT_NE(content.find("---@meta"), std::string::npos);
}

TEST(LuaDefBuilder, FuncWithNoParamsWritesEmptyParentheses) {
    LuaDefBuilder builder;
    FuncDef fd;
    fd.name = "reset";
    builder.AddNamespaceFunc("g", fd);

    const char* tmp = "_test_noparams.lua";
    builder.Write(tmp);
    std::string content = ReadFile(tmp);
    std::remove(tmp);

    EXPECT_NE(content.find("function g.reset() end"), std::string::npos);
}
