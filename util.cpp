#include <string>
#include <windows.h>
#include <stringapiset.h>

std::string to_string(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

std::wstring to_wstring(const std::string& s) {
    if (s.empty()) return L"";

    // 필요한 크기 계산
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, NULL, 0);
    if (len <= 0) return L"";

    // wstring 공간 확보
    std::wstring buf(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &buf[0], len);

    // MultiByteToWideChar는 널 문자를 포함하므로, 끝의 \0를 제거해주는 게 좋습니다.
    if (!buf.empty() && buf.back() == L'\0') {
        buf.pop_back();
    }

    return buf;
}