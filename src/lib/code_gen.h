//
// Created by WavJaby on 2026/4/5.
//

#ifndef WENYAN_LLVM_CODE_GEN_H
#define WENYAN_LLVM_CODE_GEN_H

#include "lib/console_color.h"
#include "byte_buffer.h"
#include "scope.h"

#define SCOPE_SPACE_FMT "%*s"
#define SCOPE_SPACE_VAL ctx->currentLevel << 2, ""
#define SCOPE_SPACE_LAST_VAL (ctx->currentLevel - 1) << 2, ""

#define code(format, ...) fprintf(yyout, SCOPE_SPACE_FMT format "\n", SCOPE_SPACE_VAL, ##__VA_ARGS__)

// Location prefix with compensated indent (default 3 digits per line/col number)
#define _DIGIT_COUNT(n) ((n) < 10 ? 1 : (n) < 100 ? 2 : 3)
#define _LOC_COMP \
    (3 - _DIGIT_COUNT(yylloc.first_line)) + (3 - _DIGIT_COUNT(yylloc.first_column + 1))

#define LOC_FMT "%s:%d:%d %*s|"
#define LOC_VAL inputFileRelativePath, yylloc.first_line, yylloc.first_column + 1, \
    _LOC_COMP, ""

#define compilerLog(format, ...) do { \
    if(verbose) fprintf(stdout, LOC_FMT SCOPE_SPACE_FMT "%s" format "%s",LOC_VAL, SCOPE_SPACE_VAL,  COLOR_BLUE, ##__VA_ARGS__, COLOR_RESET); \
} while (0)

#define lexerLog(format, ...) do { \
    if(lexerVerbose) fprintf(stdout, LOC_FMT SCOPE_SPACE_FMT "%s" format "%s", LOC_VAL, SCOPE_SPACE_VAL, COLOR_CYAN, ##__VA_ARGS__, COLOR_RESET); \
} while (0)

#define buffPrintln(buff, format, ...) do { \
    if (stdinMode && buff != &constBuff) \
        printf("%s" SCOPE_SPACE_FMT format "%s\n", COLOR_GREEN, SCOPE_SPACE_VAL, ##__VA_ARGS__, COLOR_RESET); \
    byteBufferWriteFormat(buff, SCOPE_SPACE_FMT format "\n", SCOPE_SPACE_VAL, ##__VA_ARGS__); \
} while(0)

#define buffPrintlnS(buff, format, ...) do { \
    if (stdinMode && buff != &constBuff) \
        printf("%s" SCOPE_SPACE_FMT format "%s\n", COLOR_GREEN, SCOPE_SPACE_LAST_VAL, ##__VA_ARGS__, COLOR_RESET); \
    byteBufferWriteFormat(buff, SCOPE_SPACE_FMT format "\n", SCOPE_SPACE_LAST_VAL, ##__VA_ARGS__); \
} while(0)

#endif //WENYAN_LLVM_CODE_GEN_H
