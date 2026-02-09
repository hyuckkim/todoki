#pragma once
#include <string>

template <class T>
inline void SafeRelease(T** ppT) {
    if (ppT && *ppT) {
        (*ppT)->Release();
        *ppT = nullptr;
    }
}
std::string to_string(const std::wstring& wstr);
std::wstring to_wstring(const std::string& s);