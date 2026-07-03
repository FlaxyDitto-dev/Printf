#ifdef _WIN32
// ============================================================================
// NATIVE WINDOWS ENGINE PROFILE
// ============================================================================
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <shellapi.h>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "kernel32.lib")

// Decodes character escapes, hex (\x1B), and octal (\033) for ANSI parsing
void process_escape_and_print(HANDLE hOut, const wchar_t* src) {
    if (!src) return;
    
    // Allocate local buffer matching max possible size
    size_t len = 0;
    while (src[len]) len++;
    
    wchar_t* dest = (wchar_t*)HeapAlloc(GetProcessHeap(), 0, (len + 1) * sizeof(wchar_t));
    if (!dest) return;
    
    size_t i = 0;
    size_t j = 0;
    
    while (src[i] != L'\0') {
        if (src[i] == L'\\' && src[i + 1] != L'\0') {
            wchar_t next = src[i + 1];
            if (next == L'n') {
                dest[j++] = L'\n';
                i += 2;
            } else if (next == L't') {
                dest[j++] = L'\t';
                i += 2;
            } else if (next == L'r') {
                dest[j++] = L'\r';
                i += 2;
            } else if (next == L'x' || next == L'X') {
                // Hex Escape Parser (\x1b)
                i += 2;
                unsigned int val = 0;
                int count = 0;
                while (count < 2 && src[i] != L'\0') {
                    wchar_t ch = src[i];
                    unsigned int digit = 0xFF;
                    if (ch >= L'0' && ch <= L'9') digit = ch - L'0';
                    else if (ch >= L'a' && ch <= L'f') digit = 10 + (ch - L'a');
                    else if (ch >= L'A' && ch <= L'F') digit = 10 + (ch - L'A');
                    
                    if (digit == 0xFF) break;
                    val = (val << 4) | digit;
                    i++;
                    count++;
                }
                dest[j++] = (wchar_t)val;
            } else if (next >= L'0' && next <= L'7') {
                // Octal Escape Parser (\033)
                i += 1;
                unsigned int val = 0;
                int count = 0;
                while (count < 3 && src[i] != L'\0') {
                    wchar_t ch = src[i];
                    if (ch >= L'0' && ch <= L'7') {
                        val = (val << 3) + (ch - L'0');
                        i++;
                        count++;
                    } else {
                        break;
                    }
                }
                dest[j++] = (wchar_t)val;
            } else {
                dest[j++] = src[i];
                i++;
            }
        } else {
            dest[j++] = src[i];
            i++;
        }
    }
    
    dest[j] = L'\0';
    
    DWORD written = 0;
    WriteConsoleW(hOut, dest, (DWORD)j, &written, NULL);
    HeapFree(GetProcessHeap(), 0, dest);
}

int main() {
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return 1;
    
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) {
        LocalFree(argv);
        return 1;
    }
    
    // Virtual Terminal Processing enables native ANSI sequences on Windows 10/11
    DWORD dwMode = 0;
    if (GetConsoleMode(hOut, &dwMode)) {
        dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(hOut, dwMode);
    }
    
    // Process arguments sequentially, preserving whitespace separation
    for (int i = 1; i < argc; i++) {
        process_escape_and_print(hOut, argv[i]);
        if (i < argc - 1) {
            DWORD written = 0;
            WriteConsoleW(hOut, L" ", 1, &written, NULL);
        }
    }
    
    LocalFree(argv);
    return 0;
}

#else
// ============================================================================
// INDEPENDENT POSIX ENGINE PROFILE (-nostdlib)
// ============================================================================

// Primitive Types declarations to remain independent of stdint.h
typedef unsigned long  size_t;
typedef long           ssize_t;

extern "C" {
    // Platform agnostic Naked Syscall Wrappers
    #if defined(__x86_64__)
    __attribute__((naked)) ssize_t sys_call_write(int fd, const void* buf, size_t count, long flavor) {
        __asm__ __volatile__ (
            ".intel_syntax noprefix\n"
            "mov rax, rdi\n"    // shift args to match syscall conventions dynamically
            "mov rdi, rsi\n"
            "mov rsi, rdx\n"
            "mov rdx, rcx\n"
            "cmp r8, 0\n"       // check system flavor: 0 = Linux, 1 = Mach-O (macOS)
            "je .linux_sys\n"
            "mov rax, 0x2000004\n" // macOS sys_write
            "syscall\n"
            "ret\n"
            ".linux_sys:\n"
            "mov rax, 1\n"         // Linux sys_write
            "syscall\n"
            "ret\n"
            ".att_syntax\n"     // Reset safely back to default AT&T compiler parsing
        );
    }

    __attribute__((naked)) void sys_call_exit(int code, long flavor) {
        __asm__ __volatile__ (
            ".intel_syntax noprefix\n"
            "mov rax, rdi\n"
            "cmp rsi, 0\n"
            "je .linux_exit\n"
            "mov rax, 0x2000001\n" // macOS sys_exit
            "syscall\n"
            "ret\n"
            ".linux_exit:\n"
            "mov rax, 60\n"        // Linux sys_exit
            "syscall\n"
            "ret\n"
            ".att_syntax\n"
        );
    }
    #define SYS_FLAVOR_LINUX 0
    #define SYS_FLAVOR_MACOS 1

    #elif defined(__aarch64__)
    // Apple Silicon AArch64 raw syscall engine
    __attribute__((naked)) ssize_t sys_call_write(int fd, const void* buf, size_t count, long flavor) {
        __asm__ (
            "mov x8, x0\n"
            "mov x0, x1\n"
            "mov x1, x2\n"
            "mov x2, x3\n"
            "mov x16, #4\n"       // macOS Mach-O write identifier
            "svc #0x80\n"
            "ret\n"
        );
    }

    __attribute__((naked)) void sys_call_exit(int code, long flavor) {
        __asm__ (
            "mov x8, x0\n"
            "mov x0, x1\n"
            "mov x16, #1\n"       // macOS Mach-O exit identifier
            "svc #0x80\n"
            "ret\n"
        );
    }
    #define SYS_FLAVOR_LINUX 0
    #define SYS_FLAVOR_MACOS 1
    #endif

    void posix_main(int argc, char** argv, long flavor);
    
    // Linux Entry Point
    void _start(long* ap) {
        int argc = (int)(*ap);
        char** argv = (char**)(ap + 1);
        posix_main(argc, argv, SYS_FLAVOR_LINUX);
    }

    // Mach-O macOS Entry Point
    void start(long* ap) {
        int argc = (int)(*ap);
        char** argv = (char**)(ap + 1);
        posix_main(argc, argv, SYS_FLAVOR_MACOS);
    }
}

void process_escape_and_print_posix(const char* src, long flavor) {
    if (!src) return;
    
    size_t len = 0;
    while (src[len]) len++;
    
    // Allocation-free stacked conversion array
    char buf[4096];
    size_t i = 0;
    size_t j = 0;
    
    while (src[i] != '\0' && j < 4095) {
        if (src[i] == '\\' && src[i + 1] != '\0') {
            char next = src[i + 1];
            if (next == 'n') {
                buf[j++] = '\n';
                i += 2;
            } else if (next == 't') {
                buf[j++] = '\t';
                i += 2;
            } else if (next == 'r') {
                buf[j++] = '\r';
                i += 2;
            } else if (next == 'x' || next == 'X') {
                i += 2;
                unsigned char val = 0;
                int count = 0;
                while (count < 2 && src[i] != '\0') {
                    char ch = src[i];
                    unsigned char digit = 0xFF;
                    if (ch >= '0' && ch <= '9') digit = ch - '0';
                    else if (ch >= 'a' && ch <= 'f') digit = 10 + (ch - 'a');
                    else if (ch >= 'A' && ch <= 'F') digit = 10 + (ch - 'A');
                    
                    if (digit == 0xFF) break;
                    val = (val << 4) | digit;
                    i++;
                    count++;
                }
                buf[j++] = (char)val;
            } else if (next >= '0' && next <= '7') {
                i += 1;
                unsigned char val = 0;
                int count = 0;
                while (count < 3 && src[i] != '\0') {
                    char ch = src[i];
                    if (ch >= '0' && ch <= '7') {
                        val = (val << 3) + (ch - '0');
                        i++;
                        count++;
                    } else {
                        break;
                    }
                }
                buf[j++] = (char)val;
            } else {
                buf[j++] = src[i];
                i++;
            }
        } else {
            buf[j++] = src[i];
            i++;
        }
    }
    
    sys_call_write(1, buf, j, flavor);
}

void posix_main(int argc, char** argv, long flavor) {
    for (int i = 1; i < argc; i++) {
        process_escape_and_print_posix(argv[i], flavor);
        if (i < argc - 1) {
            sys_call_write(1, " ", 1, flavor);
        }
    }
    sys_call_exit(0, flavor);
}
#endif
