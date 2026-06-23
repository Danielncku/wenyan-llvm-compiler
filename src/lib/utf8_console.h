#ifndef UTF8_CONSOLE_H
#define UTF8_CONSOLE_H

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <windows.h>

__attribute__((constructor))
static inline void utf8_init(void) {
    _setmode(0, _O_BINARY);
    _setmode(1, _O_BINARY);
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
}
#else
static inline void utf8_init(void) {
}
#endif

#endif // UTF8_CONSOLE_H
