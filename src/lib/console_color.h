//
// Created by WavJaby on 2026/4/5.
//

#ifndef WENYAN_LLVM_CONSOLE_COLOR_H
#define WENYAN_LLVM_CONSOLE_COLOR_H

#include <stdbool.h>

extern const char* COLOR_RED;
extern const char* COLOR_YELLOW;
extern const char* COLOR_GREEN;
extern const char* COLOR_CYAN;
extern const char* COLOR_BLUE;
extern const char* COLOR_RESET;

void color_init(bool enabled);

#endif //WENYAN_LLVM_CONSOLE_COLOR_H
