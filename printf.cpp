#include <windows.h>
#include <string>
void process_escape(const std::wstring& str) {
    std::wstring out;
    out.reserve(str.length());
    for(size_t i = 0; i < str.length(); ++i) {
        if(str[i] == L'\\' && i + 1 < str.length()) {
            if(str[i+1] == L'x' && i + 3 < str.length()) {
                if(str.substr(i+2, 2) == L"1b" || str.substr(i+2, 2) == L"1B") {
                    out += (wchar_t)27; i += 3; continue;
                }
            }
            if(str[i+1] == L'0' && i + 3 < str.length()) {
                if(str.substr(i+2, 2) == L"33") {
                    out += (wchar_t)27; i += 3; continue;
                }
            }
            switch(str[++i]) {
                case L'n': out += L'\n'; break;
                case L't': out += L'\t'; break;
                case L'r': out += L'\r'; break;
                case L'\\': out += L'\\'; break;
                default: out += L'\\'; out += str[i];
            }
        } else { out += str[i]; }
    }
    DWORD written;
    WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), out.c_str(), (DWORD)out.length(), &written, NULL);
}
int main() {
    int argc;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if(argc < 2 || !argv) return 0;
    for (int i = 1; i < argc; ++i) {
        process_escape(argv[i]);
    }
    LocalFree(argv);
    return 0;
}
