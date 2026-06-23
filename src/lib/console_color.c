//
// Created by WavJaby on 2026/6/1.
//

#include "console_color.h"

const char* COLOR_RED = "";
const char* COLOR_YELLOW = "";
const char* COLOR_GREEN = "";
const char* COLOR_CYAN = "";
const char* COLOR_BLUE = "";
const char* COLOR_RESET = "";

void color_init(bool enabled) {
    if (!enabled) return;
    COLOR_RED = "\033[31m";
    COLOR_YELLOW = "\033[33m";
    COLOR_GREEN = "\033[32m";
    COLOR_CYAN = "\033[36m";
    COLOR_BLUE = "\033[34m";
    COLOR_RESET = "\033[0m";
}
