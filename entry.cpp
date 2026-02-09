#include <windows.h>
#include <string>
#include <filesystem>
#include "util.h"

namespace fs = std::filesystem;

std::string entryFile = "main.lua";

HRESULT ReadEnteryFromArgs() {
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return E_FAIL;

    if (argv && argc > 1) {
        entryFile = to_string(argv[1]);
        printf("[Engine] Entry script changed to: %s\n", entryFile.c_str());
        LocalFree(argv);
        return S_OK;
    }

    if (!fs::exists(entryFile)) {
        printf("[Engine Error] Entry script '%s' not found!\n", entryFile.c_str());
        LocalFree(argv);
        return E_INVALIDARG;
    }
    LocalFree(argv);
    return S_OK;
}

const char* GetEntryFile() {
    return entryFile.c_str();
}