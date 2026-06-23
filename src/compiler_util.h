#ifndef COMPILER_UTIL_H
#define COMPILER_UTIL_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "lib/console_color.h"
#include "lib/byte_buffer.h"

typedef struct YYLTYPE {
    int first_line;
    int first_column;
    int last_line;
    int last_column;

    int first_column_byte;
    int last_column_byte;
    int line_start_offset;
} YYLTYPE;

# define YYLLOC_DEFAULT(Current, Rhs, N)                                    \
    do                                                                      \
      if (N)                                                                \
        {                                                                   \
          (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;            \
          (Current).first_column = YYRHSLOC (Rhs, 1).first_column;          \
          (Current).last_line    = YYRHSLOC (Rhs, N).last_line;             \
          (Current).last_column  = YYRHSLOC (Rhs, N).last_column;           \
          (Current).first_column_byte = YYRHSLOC (Rhs, 1).first_column_byte;\
          (Current).last_column_byte = YYRHSLOC (Rhs, N).last_column_byte;  \
          (Current).line_start_offset = YYRHSLOC (Rhs, 1).line_start_offset;\
        }                                                                   \
      else                                                                  \
        {                                                                   \
          (Current).first_line   = (Current).last_line   =                  \
            YYRHSLOC (Rhs, 0).last_line;                                    \
          (Current).first_column = (Current).last_column =                  \
            YYRHSLOC (Rhs, 0).last_column;                                  \
        }                                                                   \
    while (0)


// Internal variable
extern int yylineno;
extern int yyleng;
extern int yychar;
extern FILE* yyout;
extern FILE* yyin;
extern int yyparse();
extern int yylex();
extern int yylex_destroy();
extern YYLTYPE yylloc;

// Custom variable
extern char *inputFilePath, *inputFileRelativePath;
extern ByteBuffer constBuff;
extern bool compileError, verbose, lexerVerbose, lexerOnly, colorEnabled;
extern int constStrCount;

#define cloneStruct(type, ptr) memcpy(malloc(sizeof(type)), ptr, sizeof(type))

/*
 * LLVM text emission is centralized here so semantic modules describe
 * instructions and control flow without duplicating formatting rules.
 * The builder borrows the destination buffer and owns no memory.
 */
typedef struct {
    ByteBuffer* output;
} IRTextBuilder;

IRTextBuilder irTextBuilder(ByteBuffer* output);
void irEmit(IRTextBuilder* builder, const char* format, ...);
void irEmitBlank(IRTextBuilder* builder);
void irEmitLabel(IRTextBuilder* builder, const char* format, ...);
void irEmitBranch(IRTextBuilder* builder, const char* format, ...);
void irEmitConditionalBranch(IRTextBuilder* builder, const char* condition,
                             const char* trueLabel, const char* falseLabel);

#define ERROR_PREFIX "%s:%d:%d: 錯誤: "
#define ERROR_TEXT_BUFFER_LEN 1024
#define ERROR_TOKEN_BUFFER_LEN 64

#define yyerroraf(format, ...) do {                                                                                 \
    compileError = true;                                                                                            \
    if (verbose) fprintf(stderr, "%s:%d\n", __FILE__, __LINE__);                                                    \
    fprintf(stderr, ERROR_PREFIX format, inputFileRelativePath, yylineno, yylloc.first_column + 1, ##__VA_ARGS__);          \
    printErrorLine(&yylloc);                                                                                        \
    YYABORT;                                                                                                        \
} while (0)

#define yyerrorf(format, ...) do {                                                                                  \
    compileError = true;                                                                                            \
    if (verbose) fprintf(stderr, "%s:%d\n", __FILE__, __LINE__);                                                    \
    fprintf(stderr, "%s" ERROR_PREFIX format "%s", COLOR_RED, inputFileRelativePath, yylineno, yylloc.first_column + 1, ##__VA_ARGS__, COLOR_RESET); \
    printErrorLine(&yylloc);                                                                                        \
} while (0)

#define yyerrorlf(format, loc, ...) do {                                                                            \
    compileError = true;                                                                                            \
    if (verbose) fprintf(stderr, "%s:%d\n", __FILE__, __LINE__);                                                    \
    fprintf(stderr, "%s" ERROR_PREFIX format "%s", COLOR_RED, inputFileRelativePath, (loc)->first_line, (loc)->first_column + 1, ##__VA_ARGS__, COLOR_RESET); \
    printErrorLine(loc);                                                                                            \
} while (0)

#define yyerrortf(format, locA, locB, ...) do {                                                                     \
    compileError = true;                                                                                            \
    if (verbose) fprintf(stderr, "%s:%d\n", __FILE__, __LINE__);                                                    \
    fprintf(stderr, "%s" ERROR_PREFIX format "%s", COLOR_RED, inputFileRelativePath, (locA)->first_line, (locA)->first_column + 1, ##__VA_ARGS__, COLOR_RESET); \
    printTypeErrorLine(locA, locB);                                                                                 \
} while (0)

# define printUnknownError() yyerrorf("遘不可解之大謬。\n")

bool isFullWidth(uint32_t c);
int calculateStringWidthUtf8(const char* strUtf8);
void printErrorLine(const YYLTYPE* loc);
void printTypeErrorLine(const YYLTYPE* locA, const YYLTYPE* locB);
void yyerror(const char* format, ...);

#endif
