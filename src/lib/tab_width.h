//
// Created by WavJaby on 2026/5/26.
//

#ifndef WENYAN_LLVM_TAB_WIDTH_H
#define WENYAN_LLVM_TAB_WIDTH_H

#include <stdio.h>

static int TAB_WIDTH = 4;

#ifdef _WIN32
#include <windows.h>

__attribute__((constructor))
static inline void get_tab_width() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;

    // Not a real console (pipe / redirect), keep default
    if (!GetConsoleScreenBufferInfo(hOut, &csbi))
        return;

    fflush(stdout);

    // Cursor X pos before print \t
    int start_x = csbi.dwCursorPosition.X;

    printf("\t");
    fflush(stdout);

    // Cursor X pos after print \t
    GetConsoleScreenBufferInfo(hOut, &csbi);
    int end_x = csbi.dwCursorPosition.X;

    // Reset cursor position and clear console
    printf("\r%*s\r", end_x - start_x, "");

    TAB_WIDTH = end_x - start_x;
}

#else
#include <termios.h>
#include <unistd.h>

// Read terminal DSR response "\033[row;colR", return col or -1 on failure
static inline int read_cursor_col() {
    char buf[32];
    int i = 0;
    char c;
    // Expect ESC [
    if (read(STDIN_FILENO, &c, 1) != 1 || c != '\033') return -1;
    if (read(STDIN_FILENO, &c, 1) != 1 || c != '[')   return -1;
    // Read until 'R'
    while (i < (int)(sizeof(buf) - 1)) {
        if (read(STDIN_FILENO, &c, 1) != 1) return -1;
        if (c == 'R') break;
        buf[i++] = c;
    }
    buf[i] = '\0';
    int row, col;
    if (sscanf(buf, "%d;%d", &row, &col) != 2) return -1;
    return col;
}

__attribute__((constructor))
static inline void get_tab_width() {
    // Not a real terminal, keep default
    if (!isatty(STDOUT_FILENO) || !isatty(STDIN_FILENO))
        return;

    struct termios oldt, rawt;
    tcgetattr(STDIN_FILENO, &oldt);
    rawt = oldt;
    // Raw mode: disable echo and line buffering
    rawt.c_lflag &= ~(ICANON | ECHO);
    rawt.c_cc[VMIN] = 1;
    rawt.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &rawt);

    // Query cursor position before \t
    ssize_t _r = write(STDOUT_FILENO, "\033[6n", 4);
    int col1 = read_cursor_col();

    // Print \t and query again
    _r = write(STDOUT_FILENO, "\t\033[6n", 5);
    int col2 = read_cursor_col();

    // Clear the \t and restore cursor
    if (col2 > 0)
        printf("\r%*s\r", col2 - 1, "");
    fflush(stdout);

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

    if (col1 > 0 && col2 > col1)
        TAB_WIDTH = col2 - col1;
}

#endif

#endif //WENYAN_LLVM_TAB_WIDTH_H
