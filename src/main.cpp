#include <windows.h>
#include <shellapi.h>
#include <string>
#include "app.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
#ifdef _DEBUG
    if (AllocConsole()) {
        SetConsoleOutputCP(CP_UTF8);
        FILE* fp;
        freopen_s(&fp, "CONOUT$", "w", stdout);
        freopen_s(&fp, "CONOUT$", "w", stderr);
        printf("Debug Console Opened\n");
    }
#endif
    // Determine entry script from command line (argv[1]) if provided
    int argc = 0;
    LPWSTR* argvw = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::string entry = "main.lua";
    if (argvw) {
        if (argc > 1) {
            int size = WideCharToMultiByte(CP_UTF8, 0, argvw[1], -1, nullptr, 0, nullptr, nullptr);
            if (size > 0) {
                std::string tmp(size, '\0');
                WideCharToMultiByte(CP_UTF8, 0, argvw[1], -1, &tmp[0], size, nullptr, nullptr);
                tmp.pop_back();
                entry = tmp;
            }
        }
        LocalFree(argvw);
    }

    App app;
    if (!app.Init(hInstance, nCmdShow, entry.c_str())) {
        return 1;
    }

    app.Run();

    return 0;
}