#pragma once
#include <stdarg.h>

#include "compiler_util.h"

inline void yyerror(const char* format, ...) {
    va_list args;
    va_start(args, format);

    fprintf(stderr, ERROR_PREFIX, inputFileRelativePath, yylloc.first_line, yylloc.first_column + 1);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    printErrorLine(&yylloc);

    va_end(args);
}

static const char* yysymbolNameCh(yysymbol_kind_t symbol) {
    switch (symbol) {
    case YYSYMBOL_NUMBER_LIT: return " 數值 ";
    case YYSYMBOL_STR_LIT: return " 字串 ";
    case YYSYMBOL_VAR_TYPE: return " 類 ";
    default: return yysymbol_name(symbol);
    }
}

static int yyreport_syntax_error(const yypcontext_t* ctx) {
#define MESSAGE_MAX_LEN 1024
    char msg[MESSAGE_MAX_LEN];
    int offset = 0;

    // expecting token
    yysymbol_kind_t lookahead = yypcontext_token(ctx);
    if (lookahead != YYSYMBOL_YYEMPTY) {
        offset += snprintf(msg, MESSAGE_MAX_LEN, "忽逢%s，殊非所期", yysymbolNameCh(lookahead));
    }

    enum { TOKENMAX = 10 };
    yysymbol_kind_t expected[TOKENMAX];
    int n = yypcontext_expected_tokens(ctx, expected, TOKENMAX);
    if (n > 0) {
        offset += snprintf(msg + offset, MESSAGE_MAX_LEN - offset, "，當得");
        for (int i = 0; i < n; ++i) {
            if (i > 0) offset += snprintf(msg + offset, MESSAGE_MAX_LEN - offset, "或");
            offset += snprintf(msg + offset, MESSAGE_MAX_LEN - offset, "%s", yysymbolNameCh(expected[i]));
        }
    }
    compileError = true;
    yyerror(msg);

    return 0;
}
